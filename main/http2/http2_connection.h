#ifndef _HTTP2_CONNECTION_H_
#define _HTTP2_CONNECTION_H_

#include <cstddef>
#include <cstdint>

#include "http2_request.h"
#include "sh2lib.h"

struct Http2Header {
    const char* name;
    const char* value;
};

class Http2Connection {
   public:
    enum class Status { failed, done, timeout };

   public:
    Http2Connection(const char* uri);

    Status connect(uint32_t timeout);
    void disconnect();

    void addRequest(const char* url, Http2Request* request, const Http2Header* headers, size_t header_count);
    Status executePendingRequests(uint32_t timeout);

   private:
    static int receive_cb(struct sh2lib_handle* handle, void* context, int stream_id, const char* data, size_t len,
                          int flags);

    static int header_cb(struct sh2lib_handle* handle, void* context, int stream_id, const char* name, size_t name_len,
                         const char* value, size_t value_len);

   private:
    const char* uri;

    bool connected{false};
    sh2lib_handle handle;

    uint32_t pending_streams{0};

   private:
    Http2Connection(const Http2Connection&) = delete;
    Http2Connection(Http2Connection&&) = delete;
    Http2Connection& operator=(const Http2Connection&) = delete;
    Http2Connection& operator=(Http2Connection&&) = delete;
};

#endif  // _HTTP2_CONNECTION_H_
