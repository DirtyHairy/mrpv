#ifndef _NETWORK_H_
#define _NETWORK_H_

namespace network {

enum class result_t { ok, wifi_timeout, wifi_disconnected, sntp_timeout };

void init();

result_t start();

void stop();

}  // namespace network

#endif  // _NETWORK_H_
