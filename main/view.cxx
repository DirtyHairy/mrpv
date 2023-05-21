#include "view.h"

#include <esp_log.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
#include <string>

#include "config.h"
#include "display/font.h"
#include "display/icon.h"

using namespace std;

namespace {

const char* TAG = "view";
constexpr size_t STRING_BUFFER_SIZE = 256;

char string_buffer[STRING_BUFFER_SIZE];

const char* format_time(uint64_t timestamp) {
    static const char* weekdays[] = {SUNDAY, MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY};

    time_t epoch = timestamp;
    struct tm* time = localtime(&epoch);

    if (!time) {
        ESP_LOGE(TAG, "localtime failed: %i", errno);
        return nullptr;
    }

    if (time->tm_wday >= sizeof(weekdays)) {
        ESP_LOGE(TAG, "invalid tm_wday=%i", time->tm_wday);
        return nullptr;
    }

    char time_formatted[64];
    time_formatted[0] = '\0';
    strftime(time_formatted, 64, FORMAT_TIME, time);

    char date_formatted[64];
    date_formatted[0] = '\0';
    strftime(date_formatted, 64, FORMAT_DATE, time);

    snprintf(string_buffer, STRING_BUFFER_SIZE, FORMAT_DATETIME, time_formatted, weekdays[time->tm_wday],
             date_formatted);

    return string_buffer;
}

bool has_error(const view::model_t& model) {
    return model.network_result != network::result_t::ok || model.connection_status != api::connection_status_t::ok ||
           model.request_status_current_power != api::request_status_t::ok ||
           model.request_status_accumulated_power != api::request_status_t::ok;
}

const char* describe_network_result(network::result_t result) {
    switch (result) {
        case network::result_t::wifi_disconnected:
        case network::result_t::wifi_timeout:
            return "wifi: connection failed";

        case network::result_t::sntp_timeout:
            return "ntp: failed to sync";

        case network::result_t::ok:
            return "ok";
    }

    return "";
}

const char* describe_connection_status(api::connection_status_t status) {
    switch (status) {
        case api::connection_status_t::error:
            return "API: connection error";

        case api::connection_status_t::pending:
            return "API: internal error";

        case api::connection_status_t::timeout:
            return "API: timeout";

        case api::connection_status_t::transfer_error:
            return "API: transfer error";

        case api::connection_status_t::ok:
            return "API: ok";
    }

    return "";
}

const char* describe_request_status(api::request_status_t status) {
    switch (status) {
        case api::request_status_t::api_error:
            return "API error";

        case api::request_status_t::http_error:
            return "HTTP error";

        case api::request_status_t::invalid_response:
            return "bad response";

        case api::request_status_t::rate_limit:
            return "rate limit";

        case api::request_status_t::timeout:
            return "timeout";

        case api::request_status_t::ok:
            return "ok";

        case api::request_status_t::no_request:
            return "no request";

        case api::request_status_t::pending:
            return "pending";
    }

    return "";
}

const char* format_power(const char* label, float power_w) {
    if (power_w < 0)
        snprintf(string_buffer, STRING_BUFFER_SIZE, "%s: -", label);
    else
        snprintf(string_buffer, STRING_BUFFER_SIZE, "%s: %.0fW", label, power_w);

    return string_buffer;
}

const char* format_accumulated_power_kwh(const char* label, float accumulated_power_kwh) {
    if (accumulated_power_kwh < 0)
        snprintf(string_buffer, STRING_BUFFER_SIZE, "%s: -", label);
    else
        snprintf(string_buffer, STRING_BUFFER_SIZE, "%s: %.1fkWh", label, accumulated_power_kwh);

    return string_buffer;
}

const char* format_charge(int32_t charge) {
    if (charge < 0) return "-";

    snprintf(string_buffer, STRING_BUFFER_SIZE, "%lu%%", min(charge, static_cast<int32_t>(100)));
    return string_buffer;
}

void draw_error_message(Adafruit_GFX& gfx, const char* message) {
    gfx.setFont(nullptr);
    gfx.writeRightJustified(400, 23, message);
}

void draw_error(Adafruit_GFX& gfx, const view::model_t& model) {
    if (!has_error(model)) return;

    if (model.network_result != network::result_t::ok)
        return draw_error_message(gfx, describe_network_result(model.network_result));

    if (model.connection_status != api::connection_status_t::ok)
        return draw_error_message(gfx, describe_connection_status(model.connection_status));

    ostringstream sstream;
    if (model.request_status_current_power != api::request_status_t::ok)
        sstream << "live data: " << describe_request_status(model.request_status_current_power);

    if (model.request_status_accumulated_power != api::request_status_t::ok) {
        if (model.request_status_current_power != api::request_status_t::ok) sstream << ", ";

        sstream << "acc data: " << describe_request_status(model.request_status_accumulated_power);
    }

    draw_error_message(gfx, sstream.str().c_str());
}

void draw_battery_segment_top(Adafruit_GFX& gfx, uint32_t x, uint32_t y, uint32_t width, uint32_t radius, bool filled) {
    if (filled) {
        gfx.fillRoundRect(x, y, width, 50, radius, 1);
        gfx.fillRect(x, y + 50 - radius, width, radius, 1);
    }
}

void draw_battery_segment_middle(Adafruit_GFX& gfx, uint32_t x, uint32_t y, uint32_t width, bool filled) {
    if (filled) gfx.fillRect(x, y, width, 50, 1);
}

void draw_battery_segment_bottom(Adafruit_GFX& gfx, uint32_t x, uint32_t y, uint32_t width, uint32_t radius,
                                 bool filled) {
    if (filled) {
        gfx.fillRoundRect(x, y, width, 50, radius, 1);
        gfx.fillRect(x, y, width, radius, 1);
    }
}

void draw_battery(Adafruit_GFX& gfx, uint32_t x, uint32_t y, uint32_t width, uint32_t radius, int32_t charge) {
    gfx.drawRoundRect(x, y, width, 225, radius, 1);

    if (charge < 0) return;

    draw_battery_segment_top(gfx, x + 5, y + 5, width - 10, 10, charge >= 75);
    draw_battery_segment_middle(gfx, x + 5, y + 60, width - 10, charge >= 50);
    draw_battery_segment_middle(gfx, x + 5, y + 115, width - 10, charge >= 25);
    draw_battery_segment_bottom(gfx, x + 5, y + 170, width - 10, 10, charge > 0);
}

void draw_status_icons(Adafruit_GFX& gfx, const view::model_t& model) {
    uint32_t x = 399;

    if (has_error(model)) {
        x -= icon::warning_width;
        gfx.drawXBitmap(x, 0, icon::warning_data, icon::warning_width, icon::warning_height, 1);
    } else {
        x -= icon::wifi_width;
        gfx.drawXBitmap(x, 1, icon::wifi_data, icon::wifi_width, icon::wifi_height, 1);
    }

    x -= 5;

    if (model.charging) {
        x -= icon::flash_width;
        gfx.drawXBitmap(x, 0, icon::flash_data, icon::flash_width, icon::flash_height, 1);

        x -= 5;
    }

    x -= icon::battery_width;

    switch (model.battery_status) {
        case view::battery_status_t::full:
            gfx.drawXBitmap(x, 4, icon::battery_full_data, icon::battery_width, icon::battery_height, 1);
            break;

        case view::battery_status_t::half:
            gfx.drawXBitmap(x, 4, icon::battery_half_data, icon::battery_width, icon::battery_height, 1);
            break;

        case view::battery_status_t::empty:
            gfx.drawXBitmap(x, 4, icon::battery_empty_data, icon::battery_width, icon::battery_height, 1);
            break;
    }
}

}  // namespace

void view::render(Adafruit_GFX& gfx, const model_t& model) {
    gfx.setTextWrap(false);

    const char* formatted_time(format_time(model.epoch));
    if (format_time(model.epoch)) {
        gfx.setFont(&font::freeSans9pt7b);
        gfx.write(0, 16, formatted_time);
    }

    draw_status_icons(gfx, model);
    draw_error(gfx, model);

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 68, format_power(LABEL_PV, model.power_pv_w));
    gfx.setFont(&font::freeSans12pt7b);
    gfx.write(0, 96, format_accumulated_power_kwh(LABEL_PV_ACCUMULATED, model.power_pv_accumulated_kwh));

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 150, format_power(LABEL_LOAD, model.load_w));
    gfx.setFont(&font::freeSans12pt7b);
    gfx.write(0, 178, format_accumulated_power_kwh(LABEL_LOAD_ACCUMULATED, model.load_accumulated_kwh));

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 234, format_accumulated_power_kwh(LABEL_SURPLUS, model.power_surplus_accumulated_kwh));

    gfx.setFont(&font::freeSans18pt7b);
    gfx.write(0, 291, format_accumulated_power_kwh(LABEL_NETWORK, model.power_network_accumulated_kwh));

    gfx.setFont(&font::freeSans18pt7b);
    gfx.writeCentered(338, 68, format_charge(model.charge));

    draw_battery(gfx, 285, 75, 115, 10, model.charge);
}
