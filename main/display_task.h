#ifndef _DISPLAY_TASK_H_
#define _DISPLAY_TASK_H_

#include <esp_event.h>

#include "view.h"

ESP_EVENT_DECLARE_BASE(DISPLAY_BASE);

namespace display_task {

enum event_t { event_display_complete };

void start();

void display(const view::model_t& model);

}  // namespace display_task

#endif  // _DISPLAY_TASK_H_
