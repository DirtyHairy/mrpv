#ifndef _API_H_
#define _API_H_

namespace api {

struct current_power_response_t {
    double ppv;
    double pload;
    double soc;
    double pgrid;
    double pbat;
};

struct accumulated_power_response_t {
    double eCharge;
    double eDischarge;
    double eGridCharge;
    double eInput;
    double eOutput;
    double epv;
};

enum class request_status_t {
    pending,
    ok,
    transfer_error,
    invalid_response,
    http_error,
    api_error,
    rate_limit,
    timeout
};

enum class connection_status_t { pending, ok, error, timeout };

connection_status_t perform_request();

request_status_t get_current_power_request_status();
request_status_t get_accumulated_power_request_status();

current_power_response_t& get_current_power_response();
accumulated_power_response_t& get_accumulated_power_response();

}  // namespace api

#endif  // _API_H_
