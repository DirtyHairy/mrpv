#include <esp_event.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include <cstring>

#include "config.h"
#include "display_task.h"
#include "esp_crt_bundle.h"
#include "network.h"
#include "sh2lib.h"
#include "view.h"

namespace {

const char *TAG = "main";

enum event_bit { display_complete = 0x01, wifi_connected = 0x02, wifi_disconnected = 0x04, sntp_sync_complete = 0x08 };

int streams_pending = 2;

void init_nvfs() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(result);
}

int rcv_cb(struct sh2lib_handle *handle, int stream_id, const char *data, size_t len, int flags) {
    switch (flags) {
        case DATA_RECV_RST_STREAM:
            ESP_LOGI(TAG, "stream %i closed", stream_id);
            streams_pending--;
            break;

        case 0: {
            std::string response(data, len);
            ESP_LOGI(TAG, "received %u bytes on stream %i: %s", len, stream_id, response.c_str());
            break;
        }
    }

    return 0;
}

void do_requests() {
    sh2lib_config_t config = {.uri = "https://newton.vercel.app", .crt_bundle_attach = esp_crt_bundle_attach};
    sh2lib_handle handle;

    ESP_LOGI(TAG, "connecting to server");

    if (sh2lib_connect(&config, &handle) != 0)
        ESP_LOGE(TAG, "failed to connect");
    else
        ESP_LOGI(TAG, "connection complete");

    sh2lib_do_get(&handle, "/api/v2/derive/x^4", rcv_cb);
    sh2lib_do_get(&handle, "/api/v2/derive/x^3", rcv_cb);

    uint64_t timestamp = esp_timer_get_time();

    while (streams_pending > 0) {
        if (esp_timer_get_time() - timestamp > 10000000) {
            ESP_LOGE(TAG, "request timeout");
            break;
        }

        /* Process HTTP2 send/receive */
        if (sh2lib_execute(&handle) != 0) {
            ESP_LOGE(TAG, "failed to process request");
            break;
        }

        vTaskDelay(1);
    }

    sh2lib_free(&handle);
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
