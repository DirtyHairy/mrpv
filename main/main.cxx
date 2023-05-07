#include <esp_event.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <freertos/event_groups.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "display_task.h"
#include "view.h"

namespace {

const char* TAG = "main";

EventGroupHandle_t event_group_handle;

enum event_bit { display_complete = 1 };

void on_display_complete(void*, esp_event_base_t, int32_t, void*) {
    xEventGroupSetBits(event_group_handle, event_bit::display_complete);
}

}  // namespace

extern "C" void app_main(void) {
    setenv("TZ", TIMEZONE, 1);
    tzset();

    event_group_handle = xEventGroupCreate();
    esp_event_loop_create_default();
    esp_event_handler_register(DISPLAY_BASE, display_task::event_display_complete, on_display_complete, nullptr);

    display_task::start();

    view::model_t model = {.connection_status = view::connection_status_t::online,
                           .battery_status = view::battery_status_t::full,
                           .charging = true,
                           .epoch = 1683357347,
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
