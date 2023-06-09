#ifndef _VIEW_H_
#define _VIEW_H_

#include <cstdint>

#include "api.h"
#include "display/adagfx.h"
#include "network.h"

namespace view {

enum class battery_status_t { full, half, empty };

struct model_t {
    battery_status_t battery_status;
    bool charging;

    network::result_t network_result;
    api::connection_status_t connection_status;
    api::request_status_t request_status_current_power;
    api::request_status_t request_status_accumulated_power;

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
