#include "http2_connection.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstring>

#include "esp_crt_bundle.h"

using namespace std;

namespace {
const char* TAG = "http2_connection";
}

int Http2Connection::receive_cb(struct sh2lib_handle* handle, void* context, int stream_id, const char* data,
                                size_t len, int flags) {
    Http2Request* request = reinterpret_cast<Http2Request*>(context);

    const size_t bytes_to_copy = min(len, request->max_len - request->len - 1);
    if (bytes_to_copy < len) ESP_LOGE(TAG, "request buffer overflow on stream %i", stream_id);

    memcpy(request->data.get() + request->len, data, bytes_to_copy);
    request->len += bytes_to_copy;

    if (flags == DATA_RECV_RST_STREAM) {
        request->state = Http2Request::State::complete;
        request->data[request->len] = '\0';
        request->connection->pending_streams--;
    }

    return 0;
}

int Http2Connection::header_cb(struct sh2lib_handle* handle, void* context, int stream_id, const char* name,
                               size_t name_len, const char* value, size_t value_len) {
    Http2Request* request = reinterpret_cast<Http2Request*>(context);

    if (strcmp(":status", name) == 0) {
        request->http_status = atoi(value);
    } else if (strcmp("date", name) == 0) {
        strncpy(request->date, value, Http2Request::MAX_DATE_LEN);
        request->date[Http2Request::MAX_DATE_LEN - 1] = '\0';
    }

    return 0;
}

Http2Connection::Http2Connection(const char* server_url) : server_url(server_url) {}

Http2Connection::Status Http2Connection::connect(uint32_t timeout) {
    sh2lib_config_t config = {.uri = server_url, .crt_bundle_attach = esp_crt_bundle_attach};

    ESP_LOGI(TAG, "connecting to %s", server_url);

    if (sh2lib_connect(&config, &handle, timeout) != ESP_OK) return Status::failed;

    connected = true;
    return Status::done;
}

void Http2Connection::disconnect() {
    if (!connected) return;

    sh2lib_free(&handle);
    connected = false;
}

void Http2Connection::addRequest(const char* url, Http2Request* request, const Http2Header* headers,
                                 size_t header_count) {
    if (!connected) return;

    nghttp2_nv nva[4 + header_count] = {
        SH2LIB_MAKE_NV(":method", "GET"),
        SH2LIB_MAKE_NV(":scheme", "https"),
        SH2LIB_MAKE_NV(":authority", handle.hostname),
        SH2LIB_MAKE_NV(":path", url),
    };

    for (int i = 0; i < header_count; i++) {
        nva[4 + i] = SH2LIB_MAKE_NV(headers[i].name, headers[i].value);
    }
    request->callback_context = {.frame_data_recv_cb = receive_cb, .header_cb = header_cb, .context = request};
    request->connection = this;

    sh2lib_do_get_with_nv(&handle, nva, header_count + 4, &request->callback_context);
    pending_streams++;
}

Http2Connection::Status Http2Connection::executePendingRequests(uint32_t timeout) {
    uint64_t timestamp = esp_timer_get_time();

    while (pending_streams > 0) {
        int64_t remaining = timeout * 1000 - esp_timer_get_time() + timestamp;

        if (remaining <= 0) {
            ESP_LOGE(TAG, "request timeout");
            return Status::timeout;
        }

        if (sh2lib_execute_sync(&handle, remaining / 1000) != 0) {
            ESP_LOGE(TAG, "request failed");
            return Status::failed;
        }
    }

    return Status::done;
}
