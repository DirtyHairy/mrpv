#ifndef _PERSISTENCE_H_
#define _PERSISTENCE_H_

#include <cstdint>

#include "view.h"

namespace persistence {

extern view::model_t last_view;

extern uint64_t ts_first_update;
extern uint64_t ts_last_update_accumulated_power;
extern uint64_t ts_last_time_sync;

extern uint8_t view_counter;

void init();

}  // namespace persistence

#endif  // _PERSISTENCE_H_
