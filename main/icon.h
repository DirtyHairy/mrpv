#ifndef _ICON_H_
#define _ICON_H_

#include <cstdint>

namespace icon {

constexpr uint16_t wifi_width = 21;
constexpr uint16_t wifi_height = 16;
extern const uint8_t* wifi_data;

constexpr uint16_t warning_width = 21;
constexpr uint16_t warning_height = 20;
extern const uint8_t* warning_data;

constexpr uint16_t flash_width = 15;
constexpr uint16_t flash_height = 19;
extern const uint8_t* flash_data;

constexpr uint16_t battery_width = 20;
constexpr uint16_t battery_height = 11;
extern const uint8_t* battery_full_data;
extern const uint8_t* battery_half_data;
extern const uint8_t* battery_empty_data;

}  // namespace icon

#endif  // _ICON_H_
