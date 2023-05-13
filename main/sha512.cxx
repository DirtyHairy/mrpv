#include "sha512.h"

#include <mbedtls/sha512.h>

#include <cstring>

const char* sha512::calculate(const void* data, size_t len) {
    static char encoded_result[129];
    static const char encoding[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    mbedtls_sha512_context ctx;
    unsigned char result[64];

    memset(encoded_result, '0', 128);
    encoded_result[128] = '\0';

    mbedtls_sha512_init(&ctx);
    mbedtls_sha512_starts(&ctx, 0);
    mbedtls_sha512_update(&ctx, static_cast<const unsigned char*>(data), len);
    mbedtls_sha512_finish(&ctx, result);
    mbedtls_sha512_free(&ctx);

    for (int i = 0; i < 64; i++) {
        encoded_result[2 * i] = encoding[result[i] >> 4];
        encoded_result[2 * i + 1] = encoding[result[i] & 0x0f];
    }

    return encoded_result;
}
