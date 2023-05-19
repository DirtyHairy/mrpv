#include <esp_event.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include <cstring>

#include "api.h"
#include "config.h"
#include "display_task.h"
#include "network.h"
#include "view.h"

namespace {

const char* TAG = "main";

void init_nvfs() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(result);
}

void run() {
    setenv("TZ", TIMEZONE, 1);
    tzset();

    display_task::start();

    init_nvfs();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    network::init();

    if (network::start() != network::result_t::ok) {
        ESP_LOGE(TAG, "network startup failed");
        return;
    }

    if (api::perform_request() == api::connection_status_t::ok) {
        if (api::get_current_power_request_status() == api::request_status_t::ok) {
            api::current_power_response_t& response = api::get_current_power_response();
            ESP_LOGI(TAG, "ppv = %f | pload = %f | soc = %f | pgrid = %f | pbat = %f", response.ppv, response.pload,
                     response.soc, response.pgrid, response.pbat);
        }
        if (api::get_accumulated_power_request_status() == api::request_status_t::ok) {
            api::accumulated_power_response_t& response = api::get_accumulated_power_response();
            ESP_LOGI(TAG, "eCharge = %f | eDischarge = %f | eGridCharge = %f | eInput = %f | eOutput = %f | epv = %f",
                     response.eCharge, response.eDischarge, response.eGridCharge, response.eInput, response.eOutput,
                     response.epv);
        }
    }

    view::model_t model = {.connection_status = view::connection_status_t::online,
                           .battery_status = view::battery_status_t::full,
                           .charging = true,
                           .epoch = static_cast<uint64_t>(time(nullptr)),
                           .power_pv_w = 7963.4,
                           .power_pv_accumulated_kwh = 16.794,
                           .load_w = 1076,
                           .load_accumulated_kwh = 10.970,
                           .power_surplus_accumulated_kwh = 3.212,
                           .power_network_accumulated_kwh = 7.723,
                           .charge = 74};

    strcpy(model.error_message, "no connection to API");

    display_task::display(model);
    network::stop();
    display_task::wait();
}

}  // namespace

extern "C" void app_main(void) {
    run();

    ESP_LOGI(TAG, "going to sleep now");

    for (auto domain : {ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_DOMAIN_VDDSDIO})
        esp_sleep_pd_config(domain, ESP_PD_OPTION_AUTO);

    esp_deep_sleep(90 * 1000000);
}
