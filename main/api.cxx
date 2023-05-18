#include "api.h"

#include <cJSON.h>
#include <esp_log.h>
#include <sys/time.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "defer.h"
#include "http2/http2_connection.h"
#include "sha512.h"

namespace {

const char* TAG = "api";

api::current_power_response_t current_power_response;
api::accumulated_power_response_t accumulated_power_response;

api::request_status_t request_status_current_power;
api::request_status_t request_status_accumulated_power;

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

cJSON* deserialize_response(const uint8_t* serialized_json, api::request_status_t& status, const char* endpoint) {
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

    return json;
}

void deserialize_current_power(const uint8_t* serialized_json) {
    cJSON* json = deserialize_response(serialized_json, request_status_current_power, AESS_API_CURRENT_POWER);
    if (!json || request_status_accumulated_power != api::request_status_t::ok) return;

    Defer deallocate_json([&]() { cJSON_Delete(json); });

    cJSON* data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!cJSON_IsObject(data)) {
        ESP_LOGE(TAG, "invalid response - no payload: %s", serialized_json);
        request_status_current_power = api::request_status_t::invalid_response;
        return;
    }

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

    current_power_response = {.ppv = cJSON_GetNumberValue(ppv),
                              .pload = cJSON_GetNumberValue(pload),
                              .soc = cJSON_GetNumberValue(soc),
                              .pgrid = cJSON_GetNumberValue(pgrid),
                              .pbat = cJSON_GetNumberValue(pbat)};
}

void deserialize_accumulated_power(const uint8_t* serialized_json) {
    cJSON* json = deserialize_response(serialized_json, request_status_accumulated_power, AESS_API_ACCUMULATED_POWER);
    if (!json || request_status_accumulated_power != api::request_status_t::ok) return;

    Defer deallocate_json([&]() { cJSON_Delete(json); });

    cJSON* data = cJSON_GetObjectItemCaseSensitive(json, "data");
    if (!cJSON_IsObject(data)) {
        ESP_LOGE(TAG, "invalid response - no payload: %s", serialized_json);
        request_status_current_power = api::request_status_t::invalid_response;
        return;
    }

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

    accumulated_power_response = {.eCharge = cJSON_GetNumberValue(eCharge),
                                  .eDischarge = cJSON_GetNumberValue(eDischarge),
                                  .eGridCharge = cJSON_GetNumberValue(eGridCharge),
                                  .eInput = cJSON_GetNumberValue(eInput),
                                  .eOutput = cJSON_GetNumberValue(eOutput),
                                  .epv = cJSON_GetNumberValue(epv)};
}

}  // namespace

api::connection_status_t api::perform_request() {
    request_status_accumulated_power = request_status_current_power = request_status_t::pending;

    const char* timestamp = get_timestamp();
    const char* secret = calculate_secret(timestamp);
    const char* date = get_date();

    Http2Connection connection(AESS_API_SERVER);
    switch (connection.connect(CONNECTION_TIMEOUT_MSEC)) {
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

    snprintf(uri, 128, "%s?sysSn=%s&queryDate=%s", AESS_API_ACCUMULATED_POWER, AESS_SERIAL, date);
    connection.addRequest(uri, &request_accumulated_power, headers, 4);

    switch (connection.executePendingRequests(REQUEST_TIMEOUT_MSEC)) {
        case Http2Connection::Status::failed:
            ESP_LOGE(TAG, "requests failed");
            request_status_accumulated_power = request_status_current_power = request_status_t::transfer_error;
            return connection_status_t::ok;

        case Http2Connection::Status::timeout:
            ESP_LOGE(TAG, "request timeout");
            request_status_accumulated_power = request_status_current_power = request_status_t::timeout;
            return connection_status_t::ok;

        default:
            break;
    }

    request_status_accumulated_power = request_status_current_power = request_status_t::ok;

    if (request_current_power.getHttpStatus() != 200) {
        ESP_LOGE(TAG, "GET for %s failed with HTTP status %li", AESS_API_CURRENT_POWER,
                 request_current_power.getHttpStatus());

        request_status_current_power = request_status_t::http_error;
    }

    if (request_accumulated_power.getHttpStatus() != 200) {
        ESP_LOGE(TAG, "GET for %s failed with HTTP status %li", AESS_API_ACCUMULATED_POWER,
                 request_accumulated_power.getHttpStatus());

        request_status_accumulated_power = request_status_t::http_error;
    }

    if (request_status_current_power == request_status_t::ok)
        deserialize_current_power(request_current_power.getData());

    if (request_status_accumulated_power == request_status_t::ok)
        deserialize_accumulated_power(request_accumulated_power.getData());

    return connection_status_t::ok;
}

api::request_status_t api::get_current_power_request_status() { return request_status_current_power; }

api::request_status_t api::get_accumulated_power_request_status() { return request_status_accumulated_power; }

api::current_power_response_t& api::get_current_power_response() { return current_power_response; }

api::accumulated_power_response_t& api::get_accumulated_power_response() { return accumulated_power_response; }
