#include "api.h"

#include <cJSON.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <sys/time.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "defer.h"
#include "http2/http2_connection.h"
#include "persistence.h"
#include "sha512.h"

namespace {

const char* TAG = "api";

esp_pm_lock_handle_t pm_lock;

api::current_power_response_t current_power_response;
api::accumulated_power_response_t accumulated_power_response;

api::request_status_t request_status_current_power;
api::request_status_t request_status_accumulated_power;

char date_from_request[Http2Request::MAX_DATE_LEN] = "";

const char* get_timestamp() {
    static char timestamp[16];
    snprintf(timestamp, 16, "%llu", time(nullptr));

    return timestamp;
}

const char* calculate_secret(const char* timestamp) {
    char secret[256];
    snprintf(secret, 256, "%s%s%s", AESS_APP_ID, AESS_SECRET, timestamp);

    return sha512::calculate(secret, strlen(secret));
}

const char* get_date() {
    static char date[11];
    time_t timeval = time(nullptr);
    strftime(date, 11, "%Y-%m-%d", localtime(&timeval));

    return date;
}

cJSON* deserialize_response(const uint8_t* serialized_json, cJSON*& data, api::request_status_t& status,
                            const char* endpoint) {
    data = nullptr;

    cJSON* json = cJSON_Parse(reinterpret_cast<const char*>(serialized_json));
    if (!json) {
        ESP_LOGE(TAG, "invalid response - bad JSON: %s", serialized_json);
        status = api::request_status_t::invalid_response;
        return nullptr;
    }

    cJSON* code_serialized = cJSON_GetObjectItemCaseSensitive(json, "code");

    if (!cJSON_IsNumber(code_serialized)) {
        ESP_LOGE(TAG, "invalid response - no code field: %s", serialized_json);
        status = api::request_status_t::invalid_response;
        return json;
    }

    int code = round(cJSON_GetNumberValue(code_serialized));

    if (code == AESS_API_ERROR_RATE_LIMIT) {
        ESP_LOGE(TAG, "%s hit rate limit", endpoint);
        status = api::request_status_t::rate_limit;
        return json;
    }

    if (code != AESS_API_OK) {
        ESP_LOGE(TAG, "%s return API error %i", endpoint, code);
        status = api::request_status_t::api_error;
    }

    data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!cJSON_IsObject(data)) {
        ESP_LOGE(TAG, "invalid response - no payload: %s", serialized_json);
        status = api::request_status_t::invalid_response;
        data = nullptr;

        return json;
    }

    return json;
}

void deserialize_current_power(const uint8_t* serialized_json) {
    cJSON* data = nullptr;
    cJSON* json = deserialize_response(serialized_json, data, request_status_current_power, AESS_API_CURRENT_POWER);
    if (!json || !data || request_status_current_power != api::request_status_t::ok) return;

    Defer deallocate_json([&]() { cJSON_Delete(json); });

    cJSON* ppv = cJSON_GetObjectItemCaseSensitive(data, "ppv");
    cJSON* pload = cJSON_GetObjectItemCaseSensitive(data, "pload");
    cJSON* soc = cJSON_GetObjectItemCaseSensitive(data, "soc");
    cJSON* pgrid = cJSON_GetObjectItemCaseSensitive(data, "pgrid");
    cJSON* pbat = cJSON_GetObjectItemCaseSensitive(data, "pbat");

    if (!cJSON_IsNumber(ppv) || !cJSON_IsNumber(pload) || !cJSON_IsNumber(soc) || !cJSON_IsNumber(pgrid) ||
        !cJSON_IsNumber(pbat)) {
        ESP_LOGE(TAG, "invalid response - invalid data schema: %s", serialized_json);
        request_status_current_power = api::request_status_t::invalid_response;
        return;
    }

    current_power_response = {.ppv = static_cast<float>(cJSON_GetNumberValue(ppv)),
                              .pload = static_cast<float>(cJSON_GetNumberValue(pload)),
                              .soc = static_cast<float>(cJSON_GetNumberValue(soc)),
                              .pgrid = static_cast<float>(cJSON_GetNumberValue(pgrid)),
                              .pbat = static_cast<float>(cJSON_GetNumberValue(pbat))};
}

void deserialize_accumulated_power(const uint8_t* serialized_json) {
    cJSON* data = nullptr;
    cJSON* json =
        deserialize_response(serialized_json, data, request_status_accumulated_power, AESS_API_ACCUMULATED_POWER);
    if (!json || !data || request_status_accumulated_power != api::request_status_t::ok) return;

    Defer deallocate_json([&]() { cJSON_Delete(json); });

    cJSON* eCharge = cJSON_GetObjectItemCaseSensitive(data, "eCharge");
    cJSON* eDischarge = cJSON_GetObjectItemCaseSensitive(data, "eDischarge");
    cJSON* eGridCharge = cJSON_GetObjectItemCaseSensitive(data, "eGridCharge");
    cJSON* eInput = cJSON_GetObjectItemCaseSensitive(data, "eInput");
    cJSON* eOutput = cJSON_GetObjectItemCaseSensitive(data, "eOutput");
    cJSON* epv = cJSON_GetObjectItemCaseSensitive(data, "epv");

    if (!cJSON_IsNumber(eCharge) || !cJSON_IsNumber(eDischarge) || !cJSON_IsNumber(eGridCharge) ||
        !cJSON_IsNumber(eInput) || !cJSON_IsNumber(eOutput) || !cJSON_IsNumber(epv)) {
        ESP_LOGE(TAG, "invalid response - invlid data schema: %s", serialized_json);
        request_status_current_power = api::request_status_t::invalid_response;
        return;
    }

    accumulated_power_response = {.eCharge = static_cast<float>(cJSON_GetNumberValue(eCharge)),
                                  .eDischarge = static_cast<float>(cJSON_GetNumberValue(eDischarge)),
                                  .eGridCharge = static_cast<float>(cJSON_GetNumberValue(eGridCharge)),
                                  .eInput = static_cast<float>(cJSON_GetNumberValue(eInput)),
                                  .eOutput = static_cast<float>(cJSON_GetNumberValue(eOutput)),
                                  .epv = static_cast<float>(cJSON_GetNumberValue(epv))};
}

}  // namespace

void api::init() { esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "api lock", &pm_lock); }

api::connection_status_t api::perform_request() {
    request_status_current_power = request_status_accumulated_power = request_status_t::pending;

    bool skip_request_accumulated_power =
        (static_cast<int64_t>(time(nullptr)) - persistence::ts_last_request_accumulated_power) <=
        RATE_LIMIT_ACCUMULATED_POWER_SEC;

    if (skip_request_accumulated_power) request_status_accumulated_power = request_status_t::no_request;

    const char* timestamp = get_timestamp();
    const char* secret = calculate_secret(timestamp);
    const char* date = get_date();

    Http2Connection connection(AESS_API_SERVER);

    esp_pm_lock_acquire(pm_lock);
    Http2Connection::Status status = connection.connect(CONNECTION_TIMEOUT_MSEC);
    esp_pm_lock_release(pm_lock);

    switch (status) {
        case Http2Connection::Status::failed:
            ESP_LOGE(TAG, "connection failed");
            return connection_status_t::error;

        case Http2Connection::Status::timeout:
            ESP_LOGE(TAG, "connection timeout");
            return connection_status_t::timeout;

        default:
            break;
    }

    Defer defer_diconnect([&]() { connection.disconnect(); });

    Http2Request request_current_power(2048);
    Http2Request request_accumulated_power(2048);

    const Http2Header headers[] = {{.name = "appId", .value = AESS_APP_ID},
                                   {.name = "timeStamp", .value = timestamp},
                                   {.name = "sign", .value = secret},
                                   {.name = "Accept", .value = "application/json"}};

    char uri[128];

    snprintf(uri, 128, "%s?sysSn=%s", AESS_API_CURRENT_POWER, AESS_SERIAL);
    connection.addRequest(uri, &request_current_power, headers, 4);

    if (!skip_request_accumulated_power) {
        snprintf(uri, 128, "%s?sysSn=%s&queryDate=%s", AESS_API_ACCUMULATED_POWER, AESS_SERIAL, date);
        connection.addRequest(uri, &request_accumulated_power, headers, 4);
    }

    switch (connection.executePendingRequests(REQUEST_TIMEOUT_MSEC)) {
        case Http2Connection::Status::failed:
            ESP_LOGE(TAG, "transfer failed");
            return connection_status_t::transfer_error;

        case Http2Connection::Status::timeout:
            ESP_LOGE(TAG, "request timeout");
            return connection_status_t::timeout;

        default:
            break;
    }

    request_status_current_power = request_status_t::ok;
    if (!skip_request_accumulated_power) request_status_accumulated_power = request_status_t::ok;

    if (request_current_power.getHttpStatus() != 200) {
        ESP_LOGE(TAG, "GET for %s failed with HTTP status %li", AESS_API_CURRENT_POWER,
                 request_current_power.getHttpStatus());

        request_status_current_power = request_status_t::http_error;
    }

    strncpy(date_from_request, request_current_power.getDate(), Http2Request::MAX_DATE_LEN - 1);
    date_from_request[Http2Request::MAX_DATE_LEN - 1] = '\0';

    if (!skip_request_accumulated_power && request_accumulated_power.getHttpStatus() != 200) {
        ESP_LOGE(TAG, "GET for %s failed with HTTP status %li", AESS_API_ACCUMULATED_POWER,
                 request_accumulated_power.getHttpStatus());

        request_status_accumulated_power = request_status_t::http_error;
    }

    if (request_status_current_power == request_status_t::ok)
        deserialize_current_power(request_current_power.getData());

    if (request_status_accumulated_power == request_status_t::ok)
        deserialize_accumulated_power(request_accumulated_power.getData());

    switch (request_status_accumulated_power) {
        case request_status_t::api_error:
        case request_status_t::invalid_response:
        case request_status_t::ok:
            persistence::ts_last_request_accumulated_power = static_cast<uint64_t>(time(nullptr));
            break;

        default:
            break;
    }

    return connection_status_t::ok;
}

api::request_status_t api::get_current_power_request_status() { return request_status_current_power; }

api::request_status_t api::get_accumulated_power_request_status() { return request_status_accumulated_power; }

api::current_power_response_t& api::get_current_power_response() { return current_power_response; }

api::accumulated_power_response_t& api::get_accumulated_power_response() { return accumulated_power_response; }

const char* api::get_date_from_request() { return date_from_request; }
