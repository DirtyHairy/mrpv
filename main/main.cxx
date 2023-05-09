#include <esp_event.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "display_task.h"
#include "view.h"

namespace {

const char* TAG = "main";

EventGroupHandle_t event_group_handle;

enum event_bit { display_complete = 0x01, wifi_connected = 0x02, wifi_disconnected = 0x04, sntp_sync_complete = 0x08 };

void on_wifi_sta_start(void*, esp_event_base_t, int32_t, void*) { ESP_ERROR_CHECK(esp_wifi_connect()); }

void on_wifi_disconnect(void*, esp_event_base_t, int32_t, void*) {
    static uint32_t connection_attempt = 0;

    if (connection_attempt >= 3) {
        ESP_LOGE(TAG, "wifi connection attempt %lu failed, giving up", connection_attempt);

        esp_wifi_stop();
        xEventGroupSetBits(event_group_handle, event_bit::wifi_disconnected);
    } else {
        ESP_LOGE(TAG, "wifi connection attempt %lu failed, retrying", connection_attempt);
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
}

void on_wifi_connect(void*, esp_event_base_t, int32_t, void*) { ESP_LOGI(TAG, "wifi connected"); }

void on_sta_got_ip(void*, esp_event_base_t, int32_t, void*) {
    ESP_LOGI(TAG, "got IP");
    xEventGroupSetBits(event_group_handle, event_bit::wifi_connected);
}

void on_display_complete(void*, esp_event_base_t, int32_t, void*) {
    xEventGroupSetBits(event_group_handle, event_bit::display_complete);
}

void on_sntp_sync_time(struct timeval*) {
    ESP_LOGI(TAG, "NTP sync complete");
    xEventGroupSetBits(event_group_handle, event_bit::sntp_sync_complete);
}

void init_nvfs() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(result);
}

void init_event_loop() {
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(
        esp_event_handler_register(DISPLAY_BASE, display_task::event_display_complete, on_display_complete, nullptr));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, on_wifi_sta_start, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, on_wifi_connect, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_wifi_disconnect, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_sta_got_ip, nullptr));
}

void init_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), WIFI_SSID);
    strcpy(reinterpret_cast<char*>(wifi_config.sta.password), WIFI_PASSWORD);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void ntp_sync_start() {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, NTP_SERVER);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_time_sync_notification_cb(on_sntp_sync_time);

    sntp_init();
}
}  // namespace

extern "C" void app_main(void) {
    setenv("TZ", TIMEZONE, 1);
    tzset();

    event_group_handle = xEventGroupCreate();

    display_task::start();

    init_nvfs();
    init_event_loop();
    init_wifi();

    xEventGroupWaitBits(event_group_handle, event_bit::wifi_connected | event_bit::wifi_disconnected, pdTRUE, pdFALSE,
                        10000 / portTICK_PERIOD_MS);

    if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
        ntp_sync_start();

        xEventGroupWaitBits(event_group_handle, event_bit::sntp_sync_complete, pdTRUE, pdFAIL,
                            10000 / portTICK_PERIOD_MS);
    } else {
        ESP_LOGI(TAG, "skipping NTP sync after wakeup from sleep");
    }

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

    xEventGroupWaitBits(event_group_handle, event_bit::display_complete, pdTRUE, pdFALSE, portMAX_DELAY);

    ESP_LOGI(TAG, "done, going to sleep now");

    for (auto domain : {ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_DOMAIN_VDDSDIO})
        esp_sleep_pd_config(domain, ESP_PD_OPTION_AUTO);

    esp_deep_sleep(20 * 1000000);
}
