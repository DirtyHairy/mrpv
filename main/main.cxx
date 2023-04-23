#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "adagfx.h"
#include "config.h"
#include "display_driver.h"
#include "esp_log.h"
#include "esp_random.h"
#include "font.h"

extern "C" void app_main(void) {
    setenv("TZ", TIMEZONE, 1);
    tzset();

    display_driver::init();

    Adafruit_GFX gfx;
    gfx.setTextWrap(false);

    gfx.fillRoundRect(285, 245, 110, 50, 10, 1);
    gfx.fillRect(285, 245, 110, 10, 1);

    gfx.fillRect(285, 190, 110, 50, 1);
    gfx.fillRect(285, 135, 110, 50, 1);

    gfx.fillRoundRect(285, 80, 110, 50, 10, 1);
    gfx.fillRect(285, 120, 110, 10, 1);

    gfx.drawRoundRect(280, 75, 120, 225, 10, 1);

    gfx.setFont(&font::freeSans9pt7b);
    gfx.write(0, 15, "23:26:32 Uhr / Samstag 22.04.2023");

    gfx.setFont(&font::freeSans18pt7b);
    gfx.writeCentered(335, 68, "100%");

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 68, "PV: 6723W");
    gfx.setFont(&font::freeSans12pt7b);
    gfx.write(0, 96, "heute: 37.9kWh");

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 152, "Last: 2096W");
    gfx.setFont(&font::freeSans12pt7b);
    gfx.write(0, 180, "heute: 13.2kWh");

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 238, "Abgabe: 1096W");
    gfx.setFont(&font::freeSans12pt7b);
    gfx.write(0, 266, "heute: 46.2kWh");
    gfx.write(170, 266, "(Abgabe)");
    gfx.setFont(&font::freeSans12pt7b);
    gfx.write(0, 294, "heute: 2.7kWh");
    gfx.write(170, 294, "(Bezug)");

    display_driver::set_mode_full();
    display_driver::display_full(gfx.getBuffer());
    display_driver::refresh_display();

    display_driver::turn_off();
}
