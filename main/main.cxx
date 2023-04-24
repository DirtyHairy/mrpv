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
#include "icon.h"

extern "C" void app_main(void) {
    setenv("TZ", TIMEZONE, 1);
    tzset();

    display_driver::init();

    Adafruit_GFX gfx;
    gfx.setTextWrap(false);

    gfx.fillRoundRect(290, 245, 105, 50, 10, 1);
    gfx.fillRect(290, 245, 105, 10, 1);

    gfx.fillRect(290, 190, 105, 50, 1);
    gfx.fillRect(290, 135, 105, 50, 1);

    gfx.fillRoundRect(290, 80, 105, 50, 10, 1);
    gfx.fillRect(290, 120, 105, 10, 1);

    gfx.drawRoundRect(285, 75, 115, 225, 10, 1);

    gfx.setFont(&font::freeSans9pt7b);
    gfx.write(0, 16, "23:26:32 Uhr / Samstag 22.04.2023");

    gfx.drawXBitmap(400 - icon::wifi_width, 1, icon::wifi_data, icon::wifi_width, icon::wifi_height, 1);
    gfx.drawXBitmap(400 - icon::wifi_width - icon::warning_width, 0, icon::warning_data, icon::warning_width,
                    icon::warning_height, 1);
    gfx.drawXBitmap(400 - icon::wifi_width - icon::warning_width - icon::flash_width, 0, icon::flash_data,
                    icon::flash_width, icon::flash_height, 1);
    gfx.drawXBitmap(400 - icon::wifi_width - icon::warning_width - icon::flash_width - icon::battery_width - 3, 5,
                    icon::battery_full_data, icon::battery_width, icon::battery_height, 1);

    gfx.setFont(nullptr);
    gfx.writeRightJustified(400, 23, "coud not connect to wifi");

    gfx.setFont(&font::freeSans18pt7b);
    gfx.writeCentered(338, 68, "100%");

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 68, "PV: 6723W");
    gfx.setFont(&font::freeSans12pt7b);
    gfx.write(0, 96, "heute: 37.9kWh");

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 150, "Last: 2096W");
    gfx.setFont(&font::freeSans12pt7b);
    gfx.write(0, 178, "heute: 13.2kWh");

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 234, "Abgabe: 34.1kWh");

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 291, "Bezug: 10.9kWh");

    display_driver::set_mode_full();
    display_driver::display_full(gfx.getBuffer());
    display_driver::refresh_display();

    display_driver::turn_off();
}
