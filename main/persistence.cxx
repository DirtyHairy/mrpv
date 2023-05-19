#include "persistence.h"

#include <esp_system.h>

RTC_NOINIT_ATTR view::model_t persistence::last_view;

RTC_NOINIT_ATTR uint64_t persistence::ts_first_update;
RTC_NOINIT_ATTR uint64_t persistence::ts_last_update_live_power;
RTC_NOINIT_ATTR uint64_t persistence::ts_last_update_preview_power;
RTC_NOINIT_ATTR uint8_t persistence::view_counter;

void persistence::init() {
    if (esp_reset_reason() == ESP_RST_DEEPSLEEP) return;

    last_view = {.connection_status = view::connection_status_t::online,
                 .battery_status = view::battery_status_t::full,
                 .charging = true,
                 .error_message = "\0",
                 .epoch = 0,
                 .power_pv_w = -1,
                 .power_pv_accumulated_kwh = -1,
                 .load_w = -1,
                 .load_accumulated_kwh = -1,
                 .power_surplus_accumulated_kwh = -1,
                 .power_network_accumulated_kwh = -1,
                 .charge = -1};

    ts_first_update = 0;
    ts_last_update_live_power = 0;
    ts_last_update_preview_power = 0;
    view_counter = 0;
}
