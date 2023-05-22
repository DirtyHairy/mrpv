#ifndef _PERSISTENCE_H_
#define _PERSISTENCE_H_

#include <esp_netif.h>

#include <cstdint>

#include "view.h"

namespace persistence {

extern view::model_t last_view;

extern uint64_t ts_first_update;
extern uint64_t ts_last_request_accumulated_power;
extern uint64_t ts_last_time_sync;
extern uint64_t ts_last_update_current_power;
extern uint64_t ts_last_update_accumulated_power;
extern uint64_t ts_last_dhcp_update;

extern uint8_t view_counter;

extern esp_netif_ip_info_t stored_ip_info;
extern esp_netif_dns_info_t stored_dns_info_main;
extern esp_netif_dns_info_t stored_dns_info_backup;
extern esp_netif_dns_info_t stored_dns_info_fallback;

extern uint8_t stored_bssid[6];
extern bool bssid_set;

void init();

void reset_ip_info();

bool ip_info_set();

void reset_bssid();

}  // namespace persistence

#endif  // _PERSISTENCE_H_
