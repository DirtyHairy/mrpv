#ifndef _HTTP2_REQUEST_H_
#define _HTTP2_REQUEST_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "sh2lib.h"

class Http2Connection;

class Http2Request {
    friend Http2Connection;

   public:
    enum class State { pending, complete };
    static constexpr size_t MAX_DATE_LEN = 64;

   public:
    Http2Request(size_t max_data_len);

    State getState() const;

    const uint8_t* getData() const;
    size_t getDataLen() const;

    int32_t getHttpStatus() const;
    const char* getDate() const;

   private:
    struct sh2lib_callback_context_t callback_context;

    State state{State::pending};

    int32_t http_status{-1};
    char date[MAX_DATE_LEN];

    std::unique_ptr<uint8_t[]> data;
    const size_t max_len;
    size_t len{0};

    Http2Connection* connection{nullptr};

   private:
    Http2Request(const Http2Request&) = delete;
    Http2Request(Http2Request&&) = delete;
    Http2Request& operator=(const Http2Request&) = delete;
    Http2Request& operator=(Http2Request&&) = delete;
};

#endif  // _HTTPS2_REQUEST_H_
