#include "persistence.h"

#include <esp_log.h>
#include <esp_system.h>

#include <cstring>

namespace {
const char* TAG = "persistence";
}

RTC_NOINIT_ATTR view::model_t persistence::last_view;

RTC_NOINIT_ATTR uint64_t persistence::ts_first_update;
RTC_NOINIT_ATTR uint64_t persistence::ts_last_request_accumulated_power;
RTC_NOINIT_ATTR uint64_t persistence::ts_last_time_sync;
RTC_NOINIT_ATTR uint64_t persistence::ts_last_update_current_power;
RTC_NOINIT_ATTR uint64_t persistence::ts_last_update_accumulated_power;
RTC_NOINIT_ATTR uint64_t persistence::ts_last_dhcp_update;

RTC_NOINIT_ATTR uint8_t persistence::view_counter;

RTC_NOINIT_ATTR esp_netif_ip_info_t persistence::stored_ip_info;
RTC_NOINIT_ATTR esp_netif_dns_info_t persistence::stored_dns_info_main;
RTC_NOINIT_ATTR esp_netif_dns_info_t persistence::stored_dns_info_backup;
RTC_NOINIT_ATTR esp_netif_dns_info_t persistence::stored_dns_info_fallback;

RTC_NOINIT_ATTR uint8_t persistence::stored_bssid[6];
RTC_NOINIT_ATTR bool persistence::bssid_set;

void persistence::init() {
    if (esp_reset_reason() == ESP_RST_DEEPSLEEP) return;

    ESP_LOGI(TAG, "hard reset, initializing...");

    last_view = {.battery_status = view::battery_status_t::full,
                 .charging = true,
                 .network_result = network::result_t::ok,
                 .connection_status = api::connection_status_t::ok,
                 .request_status_current_power = api::request_status_t::ok,
                 .request_status_accumulated_power = api::request_status_t::ok,
                 .epoch = 0,
                 .power_pv_w = -1,
                 .power_pv_accumulated_kwh = -1,
                 .load_w = -1,
                 .load_accumulated_kwh = -1,
                 .power_surplus_accumulated_kwh = -1,
                 .power_network_accumulated_kwh = -1,
                 .charge = -1};

    ts_first_update = 0;
    ts_last_request_accumulated_power = 0;
    ts_last_time_sync = 0;
    view_counter = 0;
    ts_last_update_current_power = 0;
    ts_last_update_accumulated_power = 0;
    ts_last_dhcp_update = 0;

    reset_ip_info();
    reset_bssid();
}

void persistence::reset_ip_info() {
    memset(&stored_ip_info, 0, sizeof(stored_ip_info));
    memset(&stored_dns_info_main, 0, sizeof(stored_dns_info_main));
    memset(&stored_dns_info_backup, 0, sizeof(stored_dns_info_backup));
    memset(&stored_dns_info_fallback, 0, sizeof(stored_dns_info_fallback));
}

bool persistence::ip_info_set() {
    return stored_ip_info.ip.addr != 0 &&
           (stored_dns_info_main.ip.u_addr.ip4.addr != 0 || stored_dns_info_backup.ip.u_addr.ip4.addr != 0 ||
            stored_dns_info_fallback.ip.u_addr.ip4.addr != 0);
}

void persistence::reset_bssid() {
    memset(stored_bssid, 0, sizeof(stored_bssid));
    bssid_set = false;
}
