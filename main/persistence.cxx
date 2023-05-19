#include "persistence.h"

#include <esp_log.h>
#include <esp_system.h>

namespace {
const char* TAG = "persistence";
}

RTC_NOINIT_ATTR view::model_t persistence::last_view;

RTC_NOINIT_ATTR uint64_t persistence::ts_first_update;
RTC_NOINIT_ATTR uint64_t persistence::ts_last_update_accumulated_power;
RTC_NOINIT_ATTR uint64_t persistence::ts_last_time_sync;
RTC_NOINIT_ATTR uint8_t persistence::view_counter;

void persistence::init() {
    ESP_LOGI(TAG, "ts = %llu %llu", persistence::ts_last_update_accumulated_power, persistence::ts_first_update);

    if (esp_reset_reason() == ESP_RST_DEEPSLEEP) return;

    ESP_LOGI(TAG, "hard reset, initializing...");

    last_view = {.connection_status = view::connection_status_t::online,
                 .battery_status = view::battery_status_t::full,
                 .charging = true,
                 .error_message = "",
                 .epoch = 0,
                 .power_pv_w = -1,
                 .power_pv_accumulated_kwh = -1,
                 .load_w = -1,
                 .load_accumulated_kwh = -1,
                 .power_surplus_accumulated_kwh = -1,
                 .power_network_accumulated_kwh = -1,
                 .charge = -1};

    ts_first_update = 0;
    ts_last_update_accumulated_power = 0;
    ts_last_time_sync = 0;
    view_counter = 0;
}
