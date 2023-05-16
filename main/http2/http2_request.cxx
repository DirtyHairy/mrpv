#include "http2_request.h"

using namespace std;

Http2Request::Http2Request(size_t max_data_len) : max_len(max_data_len) {
    date[0] = '\0';
    data = make_unique<uint8_t[]>(max_data_len);
}

Http2Request::State Http2Request::getState() const { return state; }

const uint8_t* Http2Request::getData() const { return data.get(); }

size_t Http2Request::getDataLen() const { return len; }

int32_t Http2Request::getHttpStatus() const { return http_status; }

const char* Http2Request::getDate() const { return date; }
