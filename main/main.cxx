#include <esp_event.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <sys/time.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

#include "api.h"
#include "config.h"
#include "date-rfc/rfc-1123.h"
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

void set_date_from_header(const char* header) {
    time_t tm = 0;
    istringstream sstream(header);

    sstream >> date::format_rfc1123(tm);
    if (sstream.fail()) {
        ESP_LOGE(TAG, "failed to parse date header %s", header);
        return;
    }

    timeval tv = {.tv_sec = tm};

    if (settimeofday(&tv, nullptr) == 0) {
        persistence::ts_last_time_sync = static_cast<uint64_t>(time(nullptr));
        ESP_LOGI(TAG, "updated time to %lli from server timestamp", tm);
    } else {
        ESP_LOGE(TAG, "failed to update time from server timestamp");
    }
}

void update_view() {
    current_view = persistence::last_view;

    current_view.network_result = network::start();
    if (current_view.network_result != network::result_t::ok) {
        persistence::reset_ip_info();
        return;
    };

    const uint64_t now = static_cast<uint64_t>(time(nullptr));
    current_view.epoch = now;

    current_view.connection_status = api::perform_request();
    if (current_view.connection_status != api::connection_status_t::ok) {
        persistence::reset_ip_info();
        return;
    }

    if (persistence::ts_first_update == 0) persistence::ts_first_update = now;

    current_view.request_status_current_power = api::get_current_power_request_status();
    if (current_view.request_status_current_power == api::request_status_t::ok) {
        api::current_power_response_t& response = api::get_current_power_response();

        current_view.power_pv_w = max(response.ppv, 0.f);
        current_view.load_w = max(response.pload, 0.f);
        current_view.charge = round(response.soc);

        persistence::ts_last_update_current_power = now;

        ESP_LOGI(TAG, "ppv = %f | pload = %f | soc = %f | pgrid = %f | pbat = %f", response.ppv, response.pload,
                 response.soc, response.pgrid, response.pbat);
    }

    if (current_view.request_status_current_power != api::request_status_t::timeout)
        set_date_from_header(api::get_date_from_request());

    const api::request_status_t status_accumulated_power = api::get_accumulated_power_request_status();

    if (status_accumulated_power != api::request_status_t::no_request)
        current_view.request_status_accumulated_power = status_accumulated_power;

    if (status_accumulated_power == api::request_status_t::rate_limit &&
        now - persistence::ts_first_update <= RATE_LIMIT_ACCUMULATED_POWER_SEC)
        current_view.request_status_accumulated_power = api::request_status_t::ok;

    if (status_accumulated_power == api::request_status_t::ok) {
        api::accumulated_power_response_t& response = api::get_accumulated_power_response();
        current_view.request_status_accumulated_power = status_accumulated_power;

        current_view.power_pv_accumulated_kwh = response.epv;
        current_view.load_accumulated_kwh =
            response.epv + response.eInput + response.eDischarge - response.eOutput - response.eCharge;
        current_view.power_network_accumulated_kwh = response.eInput;
        current_view.power_surplus_accumulated_kwh = response.eOutput;

        persistence::ts_last_update_accumulated_power = now;

        ESP_LOGI(TAG, "eCharge = %f | eDischarge = %f | eGridCharge = %f | eInput = %f | eOutput = %f | epv = %f",
                 response.eCharge, response.eDischarge, response.eGridCharge, response.eInput, response.eOutput,
                 response.epv);
    }

    if (now - persistence::ts_last_update_current_power > TTL_CURRENT_POWER_MINUTES * 60) {
        current_view.power_pv_w = -1;
        current_view.load_w = -1;
        current_view.charge = -1;
    }

    if (now - persistence::ts_last_update_accumulated_power > TTL_ACCUMULATED_POWER_MINUTES * 60) {
        current_view.power_pv_accumulated_kwh = -1;
        current_view.load_accumulated_kwh = -1;
        current_view.power_network_accumulated_kwh = -1;
        current_view.power_surplus_accumulated_kwh = -1;
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

    esp_deep_sleep(SLEEP_SECONDS * 1000000);
}
