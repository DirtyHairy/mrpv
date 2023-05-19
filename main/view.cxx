#include "view.h"

#include <esp_log.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
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
    static const char* weekdays[] = {MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY, SUNDAY};

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

const char* format_power(const char* label, float power_w) {
    if (power_w < 0) return "-";

    snprintf(string_buffer, STRING_BUFFER_SIZE, "%s: %.0fW", label, power_w);
    return string_buffer;
}

const char* format_accumulated_power_kwh(const char* label, float accumulated_power_kwh) {
    if (accumulated_power_kwh < 0) return "-";

    snprintf(string_buffer, STRING_BUFFER_SIZE, "%s: %.1fkWh", label, accumulated_power_kwh);
    return string_buffer;
}

const char* format_charge(int32_t charge) {
    if (charge < 0) return "-";

    snprintf(string_buffer, STRING_BUFFER_SIZE, "%lu%%", min(charge, static_cast<int32_t>(100)));
    return string_buffer;
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

void draw_battery(Adafruit_GFX& gfx, uint32_t x, uint32_t y, uint32_t width, uint32_t radius, uint32_t charge) {
    gfx.drawRoundRect(x, y, width, 225, radius, 1);

    draw_battery_segment_top(gfx, x + 5, y + 5, width - 10, 10, charge >= 75);
    draw_battery_segment_middle(gfx, x + 5, y + 60, width - 10, charge >= 50);
    draw_battery_segment_middle(gfx, x + 5, y + 115, width - 10, charge >= 25);
    draw_battery_segment_bottom(gfx, x + 5, y + 170, width - 10, 10, charge > 0);
}

void draw_status_icons(Adafruit_GFX& gfx, const view::model_t& model) {
    uint32_t x = 399;

    switch (model.connection_status) {
        case view::connection_status_t::online:
            x -= icon::wifi_width;
            gfx.drawXBitmap(x, 1, icon::wifi_data, icon::wifi_width, icon::wifi_height, 1);
            break;

        case view::connection_status_t::error:
            x -= icon::warning_width;
            gfx.drawXBitmap(x, 0, icon::warning_data, icon::warning_width, icon::warning_height, 1);
            break;
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

    if (model.error_message[0] != '\0') {
        gfx.setFont(nullptr);
        gfx.writeRightJustified(400, 23, model.error_message);
    }

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
