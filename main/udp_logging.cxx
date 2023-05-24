#include "udp_logging.h"

#include <esp_log.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <cstdarg>
#include <cstddef>
#include <cstring>

#include "config.h"

namespace {

constexpr size_t BUFFER_SIZE = 512;
constexpr size_t DATE_WIDTH = 24;
const char* DATE_FORMAT = "%d.%m.%Y %H:%M:%S";
const char* TAG = "udp_logging";

int fd{-1};
sockaddr_in server_addr;

char buffer[BUFFER_SIZE];
vprintf_like_t original_log_callback{nullptr};

extern "C" int log_hook(const char* format, va_list va) {
    if (fd >= 0) {
        struct tm lt;
        time_t tm = time(nullptr);
        localtime_r(&tm, &lt);

        memset(buffer, ' ', DATE_WIDTH);
        buffer[strftime(buffer, DATE_WIDTH, DATE_FORMAT, &lt)] = ' ';

        vsnprintf(buffer + DATE_WIDTH, BUFFER_SIZE, format, va);

        sendto(fd, buffer, strlen(buffer), 0, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
    }

    return original_log_callback(format, va);
}

}  // namespace

void udp_logging::start() {
    if (fd >= 0 || original_log_callback) return;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        ESP_LOGI(TAG, "cannot open UDP socket");
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(LOG_DEST_IP);
    server_addr.sin_port = htons(LOG_DEST_PORT);

    original_log_callback = esp_log_set_vprintf(log_hook);

    ESP_LOGI(TAG, "started UDP logging");
}

void udp_logging::stop() {
    if (fd < 0 || !original_log_callback) return;

    ESP_LOGI(TAG, "stopping UDP logging");

    close(fd);
    fd = -1;

    esp_log_set_vprintf(original_log_callback);
    original_log_callback = nullptr;
}
