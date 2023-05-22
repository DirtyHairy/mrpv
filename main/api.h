#ifndef _API_H_
#define _API_H_

namespace api {

struct current_power_response_t {
    float ppv;
    float pload;
    float soc;
    float pgrid;
    float pbat;
};

struct accumulated_power_response_t {
    float eCharge;
    float eDischarge;
    float eGridCharge;
    float eInput;
    float eOutput;
    float epv;
};

enum class request_status_t { pending, ok, invalid_response, http_error, api_error, rate_limit, no_request, timeout };

enum class connection_status_t { pending, ok, error, timeout, transfer_error };

void init();

connection_status_t perform_request();

request_status_t get_current_power_request_status();
request_status_t get_accumulated_power_request_status();

current_power_response_t& get_current_power_response();
accumulated_power_response_t& get_accumulated_power_response();

const char* get_date_from_request();

}  // namespace api

#endif  // _API_H_
