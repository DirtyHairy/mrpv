#ifndef _VIEW_H_
#define _VIEW_H_

#include <cstdint>

#include "display/adagfx.h"

namespace view {

enum class connection_status_t { online, error };

enum class battery_status_t { full, half, empty };

struct model_t {
    connection_status_t connection_status;
    battery_status_t battery_status;
    bool charging;

    char error_message[128];

    uint64_t epoch;

    float power_pv_w;
    float power_pv_accumulated_kwh;

    float load_w;
    float load_accumulated_kwh;

    float power_surplus_accumulated_kwh;
    float power_network_accumulated_kwh;

    int32_t charge;
};

void render(Adafruit_GFX& gfx, const model_t& model);

}  // namespace view

#endif  // _VIEW_H_
