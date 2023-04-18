#ifndef _DISPLAY_DRIVER_H_
#define _DISPLAY_DRIVER_H_

#include <cstdint>

namespace display_driver {

bool init();

void refresh_display();

void set_mode_full();

void turn_off();

void display_full(const uint8_t* image);

}  // namespace display_driver

#endif  // _DISPLAY_DRIVER_H_
