#include "display_task.h"

#include <esp_log.h>

#include "display_driver.h"
#include "freertos/queue.h"
#include "freertos/task.h"

ESP_EVENT_DEFINE_BASE(DISPLAY_BASE);

namespace {

const char* TAG = "display";

TaskHandle_t task_handle;
QueueHandle_t queue_handle;

void task_main(void*) {
    display_driver::init();

    ESP_LOGI(TAG, "display driver initialized, waiting for view data");

    view::model_t model;
    xQueueReceive(queue_handle, &model, portMAX_DELAY);

    ESP_LOGI(TAG, "received view data, rendering to display");

    Adafruit_GFX gfx;
    view::render(gfx, model);

    display_driver::set_mode_full();
    display_driver::display_full(gfx.getBuffer());
    display_driver::refresh_display();

    display_driver::turn_off();

    ESP_LOGI(TAG, "done");

    esp_event_post(DISPLAY_BASE, display_task::event_display_complete, nullptr, 0, portMAX_DELAY);

    vTaskDelete(nullptr);
}

}  // namespace

void display_task::start() {
    queue_handle = xQueueCreate(1, sizeof(view::model_t));
    xTaskCreate(task_main, "display_task", 4096, nullptr, 10, &task_handle);
}

void display_task::display(const view::model_t& model) { xQueueSend(queue_handle, &model, portMAX_DELAY); }
