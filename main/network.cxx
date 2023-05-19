#include "network.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>

#include <cstring>

#include "config.h"

namespace {

const char* TAG = "network";

enum event_bit { wifi_connected = 0x01, wifi_disconnected = 0x02, sntp_sync_complete = 0x04 };

bool stopping{false};

EventGroupHandle_t event_group_handle;

void on_wifi_sta_start(void*, esp_event_base_t, int32_t, void*) { ESP_ERROR_CHECK(esp_wifi_connect()); }

void on_wifi_disconnect(void*, esp_event_base_t, int32_t, void*) {
    if (stopping) return;

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

void on_sntp_sync_time(struct timeval*) {
    ESP_LOGI(TAG, "NTP sync complete");
    xEventGroupSetBits(event_group_handle, event_bit::sntp_sync_complete);
}

void register_events() {
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

void network::init() {
    event_group_handle = xEventGroupCreate();
    register_events();
}

network::result_t network::start() {
    init_wifi();

    EventBits_t bits;
    bits = xEventGroupWaitBits(event_group_handle, event_bit::wifi_connected | event_bit::wifi_disconnected, pdTRUE,
                               pdFALSE, WIFI_TIMEOUT_MSEC / portTICK_PERIOD_MS);

    if ((bits & event_bit::wifi_disconnected) != 0) return result_t::wifi_disconnected;

    if ((bits & wifi_connected) == 0) return result_t::wifi_timeout;

    ntp_sync_start();

    bits = xEventGroupWaitBits(event_group_handle, event_bit::sntp_sync_complete, pdTRUE, pdFAIL,
                               NTP_TIMEOUT_MSEC / portTICK_PERIOD_MS);

    if ((bits & event_bit::sntp_sync_complete) == 0) return result_t::sntp_timeout;

    return result_t::ok;
}

void network::stop() {
    stopping = true;

    sntp_stop();
    esp_wifi_stop();
}
