#include "network.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <freertos/event_groups.h>

#include <cstring>

#include "config.h"
#include "persistence.h"
#include "udp_logging.h"

namespace {

const char* TAG = "network";

enum event_bit { wifi_connected = 0x01, wifi_disconnected = 0x02, sntp_sync_complete = 0x04 };

bool stopping{false};

EventGroupHandle_t event_group_handle;

esp_netif_t* iface;

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
    iface = esp_netif_create_default_wifi_sta();

    if (persistence::ip_info_set()) {
        esp_netif_dhcpc_stop(iface);

        esp_netif_set_ip_info(iface, &persistence::stored_ip_info);
        esp_netif_set_dns_info(iface, ESP_NETIF_DNS_MAIN, &persistence::stored_dns_info_main);
        esp_netif_set_dns_info(iface, ESP_NETIF_DNS_BACKUP, &persistence::stored_dns_info_backup);
        esp_netif_set_dns_info(iface, ESP_NETIF_DNS_FALLBACK, &persistence::stored_dns_info_fallback);

        ESP_LOGI(TAG, "using cached DHCP lease");
    }

    esp_netif_set_hostname(iface, HOSTNAME);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), WIFI_SSID);
    strcpy(reinterpret_cast<char*>(wifi_config.sta.password), WIFI_PASSWORD);

    if (persistence::bssid_set) {
        memcpy(wifi_config.sta.bssid, persistence::stored_bssid, sizeof(persistence::stored_bssid));
        wifi_config.sta.bssid_set = true;
    }

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
    if (persistence::ts_last_dhcp_update != 0 &&
        static_cast<uint64_t>(time(nullptr)) - persistence::ts_last_dhcp_update >= 60 * TTL_DHCP_LEASE_MINUTES) {
        persistence::reset_ip_info();

        ESP_LOGI(TAG, "DHCP lease TTL expired, removing stored lease");
    }

    init_wifi();

    EventBits_t bits;
    bits = xEventGroupWaitBits(event_group_handle, event_bit::wifi_connected | event_bit::wifi_disconnected, pdTRUE,
                               pdFALSE, WIFI_TIMEOUT_MSEC / portTICK_PERIOD_MS);

    if ((bits & event_bit::wifi_disconnected) != 0) {
        persistence::reset_bssid();
        return result_t::wifi_disconnected;
    }

    if ((bits & wifi_connected) == 0) {
        persistence::reset_bssid();
        return result_t::wifi_timeout;
    }

    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    memcpy(persistence::stored_bssid, ap_info.bssid, sizeof(persistence::stored_bssid));
    persistence::bssid_set = true;

    if (persistence::ts_last_time_sync == 0 || static_cast<uint64_t>(time(nullptr)) - persistence::ts_last_time_sync >=
                                                   MAX_TIME_WITHOUT_TIME_SYNC_MINUTES * 60) {
        ntp_sync_start();

        bits = xEventGroupWaitBits(event_group_handle, event_bit::sntp_sync_complete, pdTRUE, pdFAIL,
                                   NTP_TIMEOUT_MSEC / portTICK_PERIOD_MS);

        if ((bits & event_bit::sntp_sync_complete) == 0) return result_t::sntp_timeout;

        sntp_stop();
        persistence::ts_last_time_sync = static_cast<uint64_t>(time(nullptr));
    } else {
        ESP_LOGI(TAG, "skipping NTP update");
    }

#if UDP_LOGGING
    udp_logging::start();
#endif

    if (!persistence::ip_info_set()) {
        esp_netif_get_ip_info(iface, &persistence::stored_ip_info);
        esp_netif_get_dns_info(iface, ESP_NETIF_DNS_MAIN, &persistence::stored_dns_info_main);
        esp_netif_get_dns_info(iface, ESP_NETIF_DNS_BACKUP, &persistence::stored_dns_info_backup);
        esp_netif_get_dns_info(iface, ESP_NETIF_DNS_FALLBACK, &persistence::stored_dns_info_fallback);

        persistence::ts_last_dhcp_update = static_cast<uint64_t>(time(nullptr));

        ESP_LOGI(TAG, "stored DHCP lease for reuse");
    }

    return result_t::ok;
}

void network::stop() {
    if (stopping) return;

#if UDP_LOGGING
    udp_logging::stop();
#endif

    stopping = true;

    sntp_stop();
    esp_wifi_stop();
}
