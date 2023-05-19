#include "display_task.h"

#include <esp_log.h>

// clang-format off
#include "freertos/FreeRTOS.h"
// clang-format on
#include "config.h"
#include "display/display_driver.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "persistence.h"

namespace {

const char* TAG = "display-task";

enum event_bit { display_complete = 0x01 };

TaskHandle_t task_handle;
QueueHandle_t queue_handle;
EventGroupHandle_t event_group_handle;

void task_main(void*) {
    display_driver::init();

    if (persistence::view_counter == 0) {
        ESP_LOGI(TAG, "performing full update");
        display_driver::set_mode_full();
    } else {
        ESP_LOGI(TAG, "performing partial update");
        display_driver::set_mode_partial();

        Adafruit_GFX gfx;
        view::render(gfx, persistence::last_view);

        display_driver::display_partial_old(gfx.getBuffer());
    }

    ESP_LOGI(TAG, "display driver initialized, waiting for view data");

    view::model_t model;
    xQueueReceive(queue_handle, &model, portMAX_DELAY);

    ESP_LOGI(TAG, "received view data, rendering to display");

    Adafruit_GFX gfx;
    view::render(gfx, model);

    if (display_driver::get_mode() == display_driver::mode::full)
        display_driver::display_full(gfx.getBuffer());
    else
        display_driver::display_partial_new(gfx.getBuffer());

    display_driver::refresh_display();
    persistence::view_counter = (persistence::view_counter + 1) % FULL_REFRESH_EVERY_CYCLE;

    display_driver::turn_off();

    ESP_LOGI(TAG, "done");

    xEventGroupSetBits(event_group_handle, event_bit::display_complete);

    vTaskDelete(nullptr);
}

}  // namespace

void display_task::start() {
    queue_handle = xQueueCreate(1, sizeof(view::model_t));
    event_group_handle = xEventGroupCreate();

    xTaskCreate(task_main, "display_task", 4096, nullptr, 10, &task_handle);
}

void display_task::display(const view::model_t& model) { xQueueSend(queue_handle, &model, portMAX_DELAY); }

void display_task::wait() {
    xEventGroupWaitBits(event_group_handle, event_bit::display_complete, pdTRUE, pdFAIL, portMAX_DELAY);
}
