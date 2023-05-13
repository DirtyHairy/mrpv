#include <esp_event.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <mbedtls/sha512.h>
#include <nvs_flash.h>
#include <sys/select.h>

#include <cstring>

#include "config.h"
#include "display_task.h"
#include "esp_crt_bundle.h"
#include "network.h"
#include "sh2lib.h"
#include "sha512.h"
#include "view.h"

namespace {

const char *TAG = "main";

enum event_bit { display_complete = 0x01, wifi_connected = 0x02, wifi_disconnected = 0x04, sntp_sync_complete = 0x08 };

int streams_pending = 0;

void init_nvfs() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(result);
}

int rcv_cb(struct sh2lib_handle *handle, int stream_id, const char *data, size_t len, int flags) {
    switch (flags) {
        case DATA_RECV_RST_STREAM:
            ESP_LOGI(TAG, "stream %i closed", stream_id);
            streams_pending--;
            break;

        case 0: {
            std::string response(data, len);
            ESP_LOGI(TAG, "received %u bytes on stream %i: %s", len, stream_id, response.c_str());
            break;
        }
    }

    return 0;
}

void request_current_power(const char *timestamp, const char *hash, sh2lib_handle *handle) {
    char uri[128];
    snprintf(uri, 128, "%s?sysSn=%s", AESS_API_CURRENT_POWER, AESS_SERIAL);

    const nghttp2_nv nva[] = {
        SH2LIB_MAKE_NV(":method", "GET"),
        SH2LIB_MAKE_NV(":scheme", "https"),
        SH2LIB_MAKE_NV(":authority", handle->hostname),
        SH2LIB_MAKE_NV(":path", uri),
        SH2LIB_MAKE_NV("appId", AESS_APP_ID),
        SH2LIB_MAKE_NV("timeStamp", timestamp),
        SH2LIB_MAKE_NV("sign", hash),
    };

    sh2lib_do_get_with_nv(handle, nva, sizeof(nva) / sizeof(nva[0]), rcv_cb);
    streams_pending++;
}

void request_accumulated_power(const char *timestamp, const char *date, const char *hash, sh2lib_handle *handle) {
    char uri[128];
    snprintf(uri, 128, "%s?sysSn=%s&queryDate=%s", AESS_API_ACCUMULATED_POWER, AESS_SERIAL, date);

    const nghttp2_nv nva[] = {SH2LIB_MAKE_NV(":method", "GET"),
                              SH2LIB_MAKE_NV(":scheme", "https"),
                              SH2LIB_MAKE_NV(":authority", handle->hostname),
                              SH2LIB_MAKE_NV(":path", uri),
                              SH2LIB_MAKE_NV("appId", AESS_APP_ID),
                              SH2LIB_MAKE_NV("timeStamp", timestamp),
                              SH2LIB_MAKE_NV("sign", hash),
                              SH2LIB_MAKE_NV("Accept", "application/json")};

    ESP_LOGI(TAG, "url %s", uri);
    ESP_LOGI(TAG, "timestamp %s", timestamp);
    ESP_LOGI(TAG, "hash %s", hash);

    sh2lib_do_get_with_nv(handle, nva, sizeof(nva) / sizeof(nva[0]), rcv_cb);
    streams_pending++;
}

void do_requests() {
    char current_time[16];
    snprintf(current_time, 16, "%llu", time(nullptr));

    char secret[256];
    snprintf(secret, 256, "%s%s%s", AESS_APP_ID, AESS_SECRET, current_time);
    const char *hash = sha512::calculate(secret, strlen(secret));

    const char *date = "2023-5-13";

    sh2lib_config_t config = {.uri = AESS_API_SERVER, .crt_bundle_attach = esp_crt_bundle_attach};
    sh2lib_handle handle;

    ESP_LOGI(TAG, "connecting to %s", AESS_API_SERVER);

    if (sh2lib_connect(&config, &handle) != 0) {
        ESP_LOGE(TAG, "failed to connect");
        return;
    } else
        ESP_LOGI(TAG, "connection complete");

    request_current_power(current_time, hash, &handle);
    request_accumulated_power(current_time, date, hash, &handle);

    int sockfd;
    uint64_t timestamp;
    const uint64_t limit = 10000000;
    if (sh2lib_get_sockfd(&handle, &sockfd) != ESP_OK) {
        ESP_LOGE(TAG, "failed to get socket handle");
        goto cleanup;
    }

    ESP_LOGI(TAG, "fd is %i", sockfd);
    timestamp = esp_timer_get_time();

    while (streams_pending > 0) {
        int64_t remaining = limit - esp_timer_get_time() + timestamp;
        if (remaining <= 0) {
            ESP_LOGE(TAG, "request timeout");
            break;
        }

        timeval timeout = {.tv_sec = remaining / 1000000, .tv_usec = static_cast<suseconds_t>(remaining % 1000000)};

        fd_set fds, fds_error;

        if (nghttp2_session_want_write(handle.http2_sess)) {
            FD_ZERO(&fds);
            FD_ZERO(&fds_error);
            FD_SET(sockfd, &fds);
            FD_SET(sockfd, &fds_error);

            if (select(sockfd + 1, nullptr, &fds, &fds_error, &timeout) == 0) continue;

            if (nghttp2_session_send(handle.http2_sess) != 0) {
                ESP_LOGE(TAG, "HTTP2 session send failed");
                break;
            }
        } else if (nghttp2_session_want_read(handle.http2_sess)) {
            FD_ZERO(&fds);
            FD_ZERO(&fds_error);
            FD_SET(sockfd, &fds);
            FD_SET(sockfd, &fds_error);

            if (select(sockfd + 1, &fds, nullptr, &fds_error, &timeout) == 0) continue;

            if (nghttp2_session_recv(handle.http2_sess) != 0) {
                ESP_LOGE(TAG, "HTTP2 session recv failed");
                break;
            }
        } else {
            ESP_LOGE(TAG, "HTTP2 session stalled");
            break;
        }
    }

cleanup:
    sh2lib_free(&handle);
}

}  // namespace

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

    do_requests();

    network::stop();

    view::model_t model = {.connection_status = view::connection_status_t::online,
                           .battery_status = view::battery_status_t::full,
                           .charging = true,
                           .epoch = static_cast<uint64_t>(time(nullptr)),
                           .power_pv = 7963.4,
                           .power_pv_accumulated = 16794,
                           .load = 1076,
                           .load_accumulated = 10970,
                           .power_surplus_accumulated = 3212,
                           .power_network_accumulated = 7723,
                           .charge = 74};

    strcpy(model.error_message, "no connection to API");

    display_task::display(model);
}

extern "C" void app_main(void) {
    run();

    ESP_LOGI(TAG, "going to sleep now");

    for (auto domain : {ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_DOMAIN_VDDSDIO})
        esp_sleep_pd_config(domain, ESP_PD_OPTION_AUTO);

    esp_deep_sleep(90 * 1000000);
}
