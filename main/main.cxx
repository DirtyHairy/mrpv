#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "adagfx.h"
#include "config.h"
#include "display_driver.h"
#include "esp_log.h"
#include "esp_random.h"
#include "font.h"
#include "icon.h"
#include "view.h"

extern "C" void app_main(void) {
    setenv("TZ", TIMEZONE, 1);
    tzset();

    display_driver::init();

    Adafruit_GFX gfx;
    gfx.setTextWrap(false);

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

    view::render(gfx, model);

    display_driver::set_mode_full();
    display_driver::display_full(gfx.getBuffer());
    display_driver::refresh_display();

    display_driver::turn_off();
}
