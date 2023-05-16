#include <esp_event.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <mbedtls/sha512.h>
#include <nvs_flash.h>
#include <sys/select.h>

#include <cstring>

#include "config.h"
#include "display_task.h"
#include "esp_crt_bundle.h"
#include "http2/http2_connection.h"
#include "http2/sh2lib.h"
#include "network.h"
#include "sha512.h"
#include "view.h"

namespace {

const char *TAG = "main";

enum event_bit { display_complete = 0x01, wifi_connected = 0x02, wifi_disconnected = 0x04, sntp_sync_complete = 0x08 };

void init_nvfs() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(result);
}

void do_requests() {
    char current_time[16];
    snprintf(current_time, 16, "%llu", time(nullptr));

    char secret[256];
    snprintf(secret, 256, "%s%s%s", AESS_APP_ID, AESS_SECRET, current_time);
    const char *hash = sha512::calculate(secret, strlen(secret));

    char date[11];
    time_t timeval = time(nullptr);
    strftime(date, 11, "%Y-%m-%d", localtime(&timeval));

    Http2Connection connection(AESS_API_SERVER);
    if (connection.connect(10000) != Http2Connection::Status::done) {
        ESP_LOGE(TAG, "connection failed");
        return;
    }

    Http2Request request_current_power(2048);
    Http2Request request_accumulated_power(2048);

    const Http2Header headers[] = {{.name = "appId", .value = AESS_APP_ID},
                                   {.name = "timeStamp", .value = current_time},
                                   {.name = "sign", .value = hash},
                                   {.name = "Accept", .value = "application/json"}};

    char uri[128];

    snprintf(uri, 128, "%s?sysSn=%s", AESS_API_CURRENT_POWER, AESS_SERIAL);
    connection.addRequest(uri, &request_current_power, headers, 4);

    snprintf(uri, 128, "%s?sysSn=%s&queryDate=%s", AESS_API_ACCUMULATED_POWER, AESS_SERIAL, date);
    connection.addRequest(uri, &request_accumulated_power, headers, 4);

    if (connection.executePendingRequests(20000) != Http2Connection::Status::done) {
        ESP_LOGE(TAG, "requests failed");
    } else {
        ESP_LOGI(TAG, "%li %s %s", request_current_power.getHttpStatus(), request_current_power.getDate(),
                 request_current_power.getData());
        ESP_LOGI(TAG, "%li %s %s", request_accumulated_power.getHttpStatus(), request_accumulated_power.getDate(),
                 request_accumulated_power.getData());
    }

    connection.disconnect();
}

}  // namespace

void run() {
    setenv("TZ", TIMEZONE, 1);
    tzset();

    display_task::start();

    init_nvfs();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    network::init();

    if (network::start() != network::result_t::ok) {
        ESP_LOGE(TAG, "network startup failed");
        return;
    }

    do_requests();

    network::stop();

    view::model_t model = {.connection_status = view::connection_status_t::online,
                           .battery_status = view::battery_status_t::full,
                           .charging = true,
                           .epoch = static_cast<uint64_t>(time(nullptr)),
                           .power_pv = 7963.4,
                           .power_pv_accumulated = 16794,
                           .load = 1076,
                           .load_accumulated = 10970,
                           .power_surplus_accumulated = 3212,
                           .power_network_accumulated = 7723,
                           .charge = 74};

    strcpy(model.error_message, "no connection to API");

    display_task::display(model);
}

extern "C" void app_main(void) {
    run();

    ESP_LOGI(TAG, "going to sleep now");

    for (auto domain : {ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_DOMAIN_VDDSDIO})
        esp_sleep_pd_config(domain, ESP_PD_OPTION_AUTO);

    esp_deep_sleep(90 * 1000000);
}
