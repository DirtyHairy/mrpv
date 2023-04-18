#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>

#include "display_driver.h"
#include "esp_log.h"

extern "C" void app_main(void) {
    display_driver::init();

    display_driver::set_mode_full();

    uint8_t* image = static_cast<uint8_t*>(malloc(300 * 50));

    for (size_t y = 0; y < 300; y++)
        for (size_t x = 0; x < 50; x++) image[x + y * 50] = (y % 4 == 0) ? 0x00 : 0xee;

    display_driver::display_full(image);

    display_driver::refresh_display();
    display_driver::turn_off();

    free(image);
}
