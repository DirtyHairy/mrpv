#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>

#include "esp_log.h"

extern "C" void app_main(void) {
    while (true) {
        ESP_LOGI("main", "hello world");

        vTaskDelay(100);
    }
}
