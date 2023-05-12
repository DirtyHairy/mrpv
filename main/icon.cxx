#include "icon.h"

namespace {
const uint8_t wifi_data[] = {0x00, 0x1f, 0x00, 0xe0, 0xff, 0x00, 0xf8, 0xf1, 0x03, 0x1e, 0x00, 0x0f,
                             0x07, 0x00, 0x1c, 0xc3, 0x7f, 0x18, 0xf0, 0xff, 0x01, 0x78, 0xc0, 0x03,
                             0x18, 0x00, 0x03, 0x80, 0x3f, 0x00, 0xc0, 0x7f, 0x00, 0xc0, 0x60, 0x00,
                             0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x0e, 0x00};

const uint8_t warning_data[] = {0x00, 0x0e, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x11, 0x00, 0x80, 0x31, 0x00,
                                0x80, 0x20, 0x00, 0xc0, 0x60, 0x00, 0x40, 0x40, 0x00, 0x60, 0xc4, 0x00,
                                0x20, 0x84, 0x00, 0x30, 0x84, 0x01, 0x10, 0x04, 0x01, 0x18, 0x04, 0x03,
                                0x08, 0x04, 0x02, 0x0c, 0x04, 0x06, 0x04, 0x00, 0x04, 0x06, 0x04, 0x0c,
                                0x02, 0x0e, 0x08, 0x03, 0x04, 0x18, 0x01, 0x00, 0x10, 0xff, 0xff, 0x1f};

const uint8_t flash_data[] = {0x00, 0x04, 0x00, 0x06, 0x00, 0x07, 0x80, 0x03, 0xc0, 0x02, 0x60, 0x02, 0x30,
                              0x02, 0x18, 0x7f, 0x0c, 0x60, 0x06, 0x30, 0x03, 0x18, 0x7f, 0x0c, 0x20, 0x06,
                              0x20, 0x03, 0xa0, 0x01, 0xe0, 0x00, 0x70, 0x00, 0x30, 0x00, 0x10, 0x00};

const uint8_t battery_full_data[] = {0xfe, 0xff, 0x01, 0x03, 0x00, 0x03, 0xfd, 0xff, 0x02, 0xfd, 0xff,
                                     0x0e, 0xfd, 0xff, 0x0e, 0xfd, 0xff, 0x0e, 0xfd, 0xff, 0x0e, 0xfd,
                                     0xff, 0x0e, 0xfd, 0xff, 0x02, 0x03, 0x00, 0x03, 0xfe, 0xff, 0x01};

const uint8_t battery_half_data[] = {0xfe, 0xff, 0x01, 0x03, 0x00, 0x03, 0xfd, 0x01, 0x02, 0xfd, 0x01,
                                     0x0e, 0xfd, 0x01, 0x0e, 0xfd, 0x01, 0x0e, 0xfd, 0x01, 0x0e, 0xfd,
                                     0x01, 0x0e, 0xfd, 0x01, 0x02, 0x03, 0x00, 0x03, 0xfe, 0xff, 0x01};

const uint8_t battery_empty_data[] = {0xfe, 0xff, 0x01, 0x03, 0x00, 0x03, 0x01, 0x00, 0x02, 0x01, 0x00,
                                      0x0e, 0x01, 0x00, 0x0e, 0x01, 0x00, 0x0e, 0x01, 0x00, 0x0e, 0x01,
                                      0x00, 0x0e, 0x01, 0x00, 0x02, 0x03, 0x00, 0x03, 0xfe, 0xff, 0x01};

}  // namespace

namespace icon {

const uint8_t* wifi_data = ::wifi_data;
const uint8_t* warning_data = ::warning_data;
const uint8_t* flash_data = ::flash_data;
const uint8_t* battery_full_data = ::battery_full_data;
const uint8_t* battery_half_data = ::battery_half_data;
const uint8_t* battery_empty_data = ::battery_empty_data;

}  // namespace icon