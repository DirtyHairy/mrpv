#ifndef _DISPLAY_DRIVER_H_
#define _DISPLAY_DRIVER_H_

#include <cstdint>

namespace display_driver {

enum class mode { partial, full, undefined };

bool init();

void refresh_display();

void set_mode_full();
void set_mode_partial();
mode get_mode();

void turn_off();
void turn_on();

void display_full(const uint8_t* image);
void display_partial(const uint8_t* image_old, const uint8_t* image_new);
void display_partial_old(const uint8_t* image_old);
void display_partial_new(const uint8_t* image_new);

}  // namespace display_driver

#endif  // _DISPLAY_DRIVER_H_
