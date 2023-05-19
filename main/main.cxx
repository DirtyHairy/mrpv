#include <esp_event.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>

#include "api.h"
#include "config.h"
#include "display_task.h"
#include "network.h"
#include "persistence.h"
#include "view.h"

using namespace std;

namespace {

const char* TAG = "main";

view::model_t current_view;

void init_nvfs() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(result);
}

void update_view_with_error(const char* error) {
    current_view.connection_status = view::connection_status_t::error;
    strncpy(current_view.error_message, error, view::ERROR_MESSAGE_MAX_LEN - 1);

    current_view.power_pv_w = -1;
    current_view.power_pv_accumulated_kwh = -1;
    current_view.load_w = -1;
    current_view.load_accumulated_kwh = -1;
    current_view.power_surplus_accumulated_kwh = -1;
    current_view.power_network_accumulated_kwh = -1;
    current_view.charge = -1;
}

const char* describe_request_error(api::request_status_t status) {
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
            return "none";

        case api::request_status_t::pending:
            return "internal";
    }

    return "";
}

void update_view() {
    current_view = persistence::last_view;
    current_view.connection_status = view::connection_status_t::online;

    switch (network::start()) {
        case network::result_t::wifi_timeout:
        case network::result_t::wifi_disconnected:
            return update_view_with_error("wifi: connection failed");

        case network::result_t::sntp_timeout:
            return update_view_with_error("ntp: failed to sync time");

        case network::result_t::ok:
            break;
    }

    const uint64_t now = static_cast<uint64_t>(time(nullptr));
    current_view.epoch = now;

    switch (api::perform_request()) {
        case api::connection_status_t::error:
            return update_view_with_error("API: connection error");

        case api::connection_status_t::pending:
            return update_view_with_error("API: internal");

        case api::connection_status_t::timeout:
            return update_view_with_error("API: timeout");

        case api::connection_status_t::transfer_error:
            return update_view_with_error("API: transfer error");

        case api::connection_status_t::ok:
            break;
    }

    if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
        persistence::ts_first_update = now;
    }

    ostringstream api_error;
    const api::request_status_t status_current_power = api::get_current_power_request_status();
    const api::request_status_t status_accumulated_power = api::get_accumulated_power_request_status();

    if (status_current_power == api::request_status_t::ok) {
        api::current_power_response_t& response = api::get_current_power_response();

        current_view.power_pv_w = max(response.ppv, 0.f);
        current_view.load_w = max(response.pload, 0.f);
        current_view.charge = round(response.soc);

        ESP_LOGI(TAG, "ppv = %f | pload = %f | soc = %f | pgrid = %f | pbat = %f", response.ppv, response.pload,
                 response.soc, response.pgrid, response.pbat);
    } else {
        api_error << "live data: " << describe_request_error(status_current_power);
        current_view.connection_status = view::connection_status_t::error;

        current_view.power_pv_w = -1;
        current_view.load_w = -1;
        current_view.charge = -1;
    }

    if (status_accumulated_power == api::request_status_t::ok) {
        api::accumulated_power_response_t& response = api::get_accumulated_power_response();

        current_view.power_pv_accumulated_kwh = response.epv;
        current_view.load_accumulated_kwh =
            response.epv + response.eInput + response.eDischarge - response.eOutput - response.eCharge;
        current_view.power_network_accumulated_kwh = response.eInput;
        current_view.power_surplus_accumulated_kwh = response.eOutput;

        persistence::ts_last_update_accumulated_power = now;

        ESP_LOGI(TAG, "eCharge = %f | eDischarge = %f | eGridCharge = %f | eInput = %f | eOutput = %f | epv = %f",
                 response.eCharge, response.eDischarge, response.eGridCharge, response.eInput, response.eOutput,
                 response.epv);
    } else if (status_accumulated_power != api::request_status_t::rate_limit ||
               (now - persistence::ts_last_update_accumulated_power >=
                    RATE_LIMIT_ACCUMULATED_POWER_SEC + RATE_LIMIT_GRACE_TIME_SEC &&
                now - persistence::ts_first_update >= RATE_LIMIT_ACCUMULATED_POWER_SEC + RATE_LIMIT_GRACE_TIME_SEC)

    ) {
        if (status_current_power != api::request_status_t::ok) api_error << " , ";

        api_error << "accumulated data: " << describe_request_error(status_accumulated_power);
        current_view.connection_status = view::connection_status_t::error;

        current_view.power_pv_accumulated_kwh = -1;
        current_view.load_accumulated_kwh = -1;
        current_view.power_network_accumulated_kwh = -1;
        current_view.power_surplus_accumulated_kwh = -1;
    }

    if (current_view.connection_status != view::connection_status_t::online) {
        strncpy(current_view.error_message, api_error.str().c_str(), view::ERROR_MESSAGE_MAX_LEN - 1);
    }
}

}  // namespace

extern "C" void app_main(void) {
    setenv("TZ", TIMEZONE, 1);
    tzset();

    persistence::init();
    display_task::start();
    init_nvfs();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    network::init();

    update_view();

    display_task::display(current_view);
    network::stop();
    display_task::wait();

    persistence::last_view = current_view;

    ESP_LOGI(TAG, "going to sleep now");

    for (auto domain : {ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_DOMAIN_VDDSDIO})
        esp_sleep_pd_config(domain, ESP_PD_OPTION_AUTO);

    esp_deep_sleep(90 * 1000000);
}
