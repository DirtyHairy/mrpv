#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>

#include "display_driver.h"
#include "esp_log.h"
#include "esp_random.h"

extern "C" void app_main(void) {
    display_driver::init();

    uint8_t* image_old = static_cast<uint8_t*>(malloc(300 * 50));
    uint8_t* image_new = static_cast<uint8_t*>(malloc(300 * 50));
    uint8_t ctr = 0;

    while (true) {
        constexpr size_t N = 30;
        uint8_t rx[N];
        uint8_t ry[N];

        for (int i = 0; i < N; i++) {
            rx[i] = esp_random() % 47;
            ry[i] = esp_random() % 276;
        }

        for (int y = 0; y < 300; y++)
            for (int x = 0; x < 50; x++) {
                image_new[y * 50 + x] = 0xff;

                for (int i = 0; i < N; i++) {
                    if ((x >= rx[i] && x - rx[i] < 3 && y >= ry[i] && y - ry[i] < 24)) image_new[y * 50 + x] = 0x00;
                }
            }

        if ((ctr++ & 0x07) == 0x00) {
            display_driver::set_mode_full();
        } else {
            display_driver::set_mode_partial();
        }

        if (display_driver::get_mode() == display_driver::mode::full)
            display_driver::display_full(image_new);
        else
            display_driver::display_partial(image_old, image_new);

        display_driver::refresh_display();

        display_driver::turn_off();
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        display_driver::turn_on();

        uint8_t* tmp = image_new;
        image_new = image_old;
        image_old = tmp;
    }
}
