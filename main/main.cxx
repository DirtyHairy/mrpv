#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>

#include "adagfx.h"
#include "display_driver.h"
#include "esp_log.h"
#include "esp_random.h"
#include "font.h"

extern "C" void app_main(void) {
    display_driver::init();

    Adafruit_GFX gfx;

    gfx.fillCircle(335, 235, 60, 1);

    gfx.setFont(&font::freeSerif24pt7b);
    gfx.write(0, 35, "Hallo Ada!");

    gfx.setFont(&font::freeSans24pt7b);
    gfx.write(0, 80, "Hallo Ada ß!");

    gfx.setFont(&font::freeSerif18pt7b);
    gfx.write(0, 115, "Hallo Ada & Barbara!");

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 150, "Hallo Ada & Barbara!");

    gfx.setFont(&font::freeSerif12pt7b);
    gfx.write(0, 175, "Hallo Ada & Barbara!");

    gfx.setFont(&font::freeSans12pt7b);
    gfx.write(0, 200, "Hallo Ada & Barbara!");

    gfx.setFont(&font::freeSerif9pt7b);
    gfx.write(0, 220, "Hallo Ada & Barbara!");

    gfx.setFont(&font::freeSans9pt7b);
    gfx.write(0, 240, "Hallo Ada & Barbara!");

    gfx.setFont(nullptr);
    gfx.write(0, 292, "Hallo Ada & Barbara & Christian!");

    display_driver::set_mode_full();
    display_driver::display_full(gfx.getBuffer());
    display_driver::refresh_display();

    display_driver::turn_off();
}
