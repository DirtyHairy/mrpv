/*
 * SPDX-FileCopyrightText: 2027-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <nghttp2/nghttp2.h>
#include <sys/time.h>

#include "esp_tls.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This is a thin API wrapper over nghttp2 that offers simplified APIs for usage
 * in the application. The intention of this wrapper is to act as a stepping
 * stone to quickly get started with using the HTTP/2 client. Since the focus is
 * on simplicity, not all the features of HTTP/2 are supported through this
 * wrapper. Once you are fairly comfortable with nghttp2, feel free to directly
 * use nghttp2 APIs or make changes to sh2lib.c for realising your objectives.
 *
 * TODO:
 * - Allowing to query response code, content-type etc in the receive callback
 * - A simple function for per-stream header callback
 */
/**
 * @brief Handle for working with sh2lib APIs
 */
struct sh2lib_handle {
    nghttp2_session *http2_sess; /*!< Pointer to the HTTP2 session handle */
    char *hostname;              /*!< The hostname we are connected to */
    struct esp_tls *http2_tls;   /*!< Pointer to the TLS session handle */
};

/**
 * @brief sh2lib configuration structure
 */
struct sh2lib_config_t {
    const char *uri;                 /*!< Pointer to the URI that should be connected to */
    const unsigned char *cacert_buf; /*!< Pointer to the buffer containing CA certificate */
    unsigned int cacert_bytes;       /*!< Size of the CA certifiacte pointed by cacert_buf */
    esp_err_t (*crt_bundle_attach)(void *conf);
    /*!< Function pointer to esp_crt_bundle_attach. Enables the use of certification
         bundle for server verification, must be enabled in menuconfig */
};

/** Flag indicating receive stream is reset */
#define DATA_RECV_RST_STREAM 1
/** Flag indicating frame is completely received  */
#define DATA_RECV_FRAME_COMPLETE 2

/**
 * @brief Function Prototype for data receive callback
 *
 * This function gets called whenever data is received on any stream. The
 * function is also called for indicating events like frame receive complete, or
 * end of stream. The function may get called multiple times as long as data is
 * received on the stream.
 *
 * @param[in] handle     Pointer to the sh2lib handle.
 * @param[in] data       Pointer to a buffer that contains the data received.
 * @param[in] len        The length of valid data stored at the 'data' pointer.
 * @param[in] flags      Flags indicating whether the stream is reset (DATA_RECV_RST_STREAM) or
 *                       this particularly frame is completely received
 *                       DATA_RECV_FRAME_COMPLETE).
 *
 * @return The function should return 0
 */
typedef int (*sh2lib_frame_data_recv_cb_t)(struct sh2lib_handle *handle, void *context, int stream_id, const char *data,
                                           size_t len, int flags);

/**
 * @brief Function Prototype for callback to send data in PUT/POST
 *
 * This function gets called whenever nghttp2 wishes to send data, like for
 * PUT/POST requests to the server. The function keeps getting called until this
 * function sets the flag NGHTTP2_DATA_FLAG_EOF to indicate end of data.
 *
 * @param[in] handle       Pointer to the sh2lib handle.
 * @param[out] data        Pointer to a buffer that should contain the data to send.
 * @param[in] len          The maximum length of data that can be sent out by this function.
 * @param[out] data_flags  Pointer to the data flags. The NGHTTP2_DATA_FLAG_EOF
 *                         should be set in the data flags to indicate end of new data.
 *
 * @return The function should return the number of valid bytes stored in the
 * data pointer
 */
typedef int (*sh2lib_putpost_data_cb_t)(struct sh2lib_handle *handle, void *context, int stream_id, char *data,
                                        size_t len, uint32_t *data_flags);

typedef int (*sh2lib_header_cb_t)(struct sh2lib_handle *handle, void *context, int stream_id, const char *name,
                                  size_t name_len, const char *value, size_t value_len);

struct sh2lib_callback_context_t {
    sh2lib_frame_data_recv_cb_t frame_data_recv_cb;
    sh2lib_putpost_data_cb_t putpost_data_cb;
    sh2lib_header_cb_t header_cb;

    void *context;
};

/**
 * @brief Connect to a URI using HTTP/2
 *
 * This API opens an HTTP/2 connection with the provided URI. If successful, the
 * hd pointer is populated with a valid handle for subsequent communication.
 *
 * Only 'https' URIs are supported.
 *
 * @param[in]  cfg     Pointer to the sh2lib configurations of the type 'struct sh2lib_config_t'.
 * @param[out] hd      Pointer to a variable of the type 'struct sh2lib_handle'.
 * @return
 *             - ESP_OK if the connection was successful
 *             - ESP_FAIL if the connection fails
 */
int sh2lib_connect(struct sh2lib_config_t *cfg, struct sh2lib_handle *hd);

/**
 * @brief Free a sh2lib handle
 *
 * This API frees-up an sh2lib handle, thus closing any open connections that
 * may be associated with this handle, and freeing up any resources.
 *
 * @param[in] hd      Pointer to a variable of the type 'struct sh2lib_handle'.
 *
 */
void sh2lib_free(struct sh2lib_handle *hd);

int sh2lib_do_get(struct sh2lib_handle *hd, const char *path, struct sh2lib_callback_context_t *callback_context);

int sh2lib_do_post(struct sh2lib_handle *hd, const char *path, struct sh2lib_callback_context_t *callback_context);

int sh2lib_do_put(struct sh2lib_handle *hd, const char *path, struct sh2lib_callback_context_t *callback_context);

/**
 * @brief Execute send/receive on an HTTP/2 connection
 *
 * While the API sh2lib_do_get(), sh2lib_do_post() setup the requests to be
 * initiated with the server, this API performs the actual data send/receive
 * operations on the HTTP/2 connection. The callback functions are accordingly
 * called during the processing of these requests.
 *
 * @param[in] hd      Pointer to a variable of the type 'struct sh2lib_handle'
 *
 * @return
 *             - ESP_OK if the connection was successful
 *             - ESP_FAIL if the connection fails
 */
int sh2lib_execute(struct sh2lib_handle *hd);

#define SH2LIB_MAKE_NV(NAME, VALUE) \
    { (uint8_t *)NAME, (uint8_t *)VALUE, strlen(NAME), strlen(VALUE), NGHTTP2_NV_FLAG_NONE }

int sh2lib_do_get_with_nv(struct sh2lib_handle *hd, const nghttp2_nv *nva, size_t nvlen,
                          struct sh2lib_callback_context_t *callback_context);

int sh2lib_do_putpost_with_nv(struct sh2lib_handle *hd, const nghttp2_nv *nva, size_t nvlen,
                              struct sh2lib_callback_context_t *callback_context);

esp_err_t sh2lib_get_sockfd(struct sh2lib_handle *hd, int *sockfd);

int sh2lib_execute_sync(struct sh2lib_handle *hd, int32_t timeout);

#ifdef __cplusplus
}
#endif
