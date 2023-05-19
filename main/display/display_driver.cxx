#include "display_driver.h"

#include "config.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define SPI_MAX_TRANSFER_SIZE (300 * 50)

namespace {

const uint8_t lut_vcom_full[] = {
    0x00, 0x08, 0x08, 0x00, 0x00, 0x02, 0x00, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x00, 0x08, 0x08,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const uint8_t lut_ww_full[] = {
    0x50, 0x08, 0x08, 0x00, 0x00, 0x02, 0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0xA0, 0x08,
    0x08, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const uint8_t lut_bw_full[] = {
    0x50, 0x08, 0x08, 0x00, 0x00, 0x02, 0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0xA0, 0x08,
    0x08, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const uint8_t lut_wb_full[] = {
    0xA0, 0x08, 0x08, 0x00, 0x00, 0x02, 0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x50, 0x08,
    0x08, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const uint8_t lut_bb_full[] = {
    0x20, 0x08, 0x08, 0x00, 0x00, 0x02, 0x90, 0x0F, 0x0F, 0x00, 0x00, 0x01, 0x10, 0x08,
    0x08, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const uint8_t lut_vcom_partial[] = {
    0x00, 0x01, 0x20, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const uint8_t lut_ww_partial[] = {
    0x00, 0x01, 0x20, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const uint8_t lut_bw_partial[] = {
    0x20, 0x01, 0x20, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const uint8_t lut_wb_partial[] = {
    0x10, 0x01, 0x20, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const uint8_t lut_bb_partial[] = {
    0x00, 0x01, 0x20, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const char* TAG = "display_driver";

spi_device_handle_t display_handle;

display_driver::mode current_mode = display_driver::mode::undefined;

QueueHandle_t busyNotificationQueue;

void send_data_tx_polling(spi_transaction_t* tx) {
    gpio_set_level(DISPLAY_PIN_DC, 1);
    spi_device_polling_transmit(display_handle, tx);
    gpio_set_level(DISPLAY_PIN_DC, 0);
}

void send_data_tx(spi_transaction_t* tx) {
    gpio_set_level(DISPLAY_PIN_DC, 1);
    spi_device_transmit(display_handle, tx);
    gpio_set_level(DISPLAY_PIN_DC, 0);
}

void send_command(uint8_t command) {
    spi_transaction_t tx = {.flags = SPI_TRANS_USE_TXDATA, .length = 8, .tx_data = {command}};

    spi_device_polling_transmit(display_handle, &tx);
}

void send_command(uint8_t command, uint8_t data) {
    send_command(command);

    spi_transaction_t tx = {.flags = SPI_TRANS_USE_TXDATA, .length = 8, .tx_data = {data}};
    send_data_tx_polling(&tx);
}

void send_command(uint8_t command, uint8_t data1, uint8_t data2) {
    send_command(command);

    spi_transaction_t tx = {.flags = SPI_TRANS_USE_TXDATA, .length = 16, .tx_data = {data1, data2}};
    send_data_tx_polling(&tx);
}

void send_command(uint8_t command, uint8_t data1, uint8_t data2, uint8_t data3) {
    send_command(command);

    spi_transaction_t tx = {.flags = SPI_TRANS_USE_TXDATA, .length = 24, .tx_data = {data1, data2, data3}};
    send_data_tx(&tx);
}

void send_command(uint8_t command, uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4) {
    send_command(command);

    spi_transaction_t tx = {.flags = SPI_TRANS_USE_TXDATA, .length = 32, .tx_data = {data1, data2, data3, data4}};
    send_data_tx(&tx);
}

void send_command(uint8_t command, const uint8_t* data, size_t len) {
    send_command(command);

    spi_transaction_t tx = {.length = len * 8, .tx_buffer = data};
    send_data_tx(&tx);
}

void busy_isr(void*) {
    uint8_t value = 0;
    xQueueSendFromISR(busyNotificationQueue, reinterpret_cast<void*>(&value), nullptr);
}

void configure_lut(const uint8_t* lut_vcom, const uint8_t* lut_ww, const uint8_t* lut_bw, const uint8_t* lut_wb,
                   const uint8_t* lut_bb) {
    send_command(0x20, lut_vcom, 44);
    send_command(0x21, lut_ww, 42);
    send_command(0x22, lut_bw, 42);
    send_command(0x23, lut_wb, 42);
    send_command(0x24, lut_bb, 42);
}

void prepare_wait_busy() { xQueueReset(busyNotificationQueue); }

void wait_busy() {
    uint8_t value;
    if (xQueueReceive(busyNotificationQueue, &value, 10000 / portTICK_PERIOD_MS) != pdTRUE)
        ESP_LOGE(TAG, "busy flag still asserted after 10 seconds, giving up");
}
void reset_sequence() {
    for (int i = 0; i < 3; i++) {
        gpio_set_level(DISPLAY_PIN_RST, 0);
        vTaskDelay(10 / portTICK_PERIOD_MS);

        gpio_set_level(DISPLAY_PIN_RST, 1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void initialize() {
    reset_sequence();

    send_command(0x01, 0x03, 0x00, 0x2b, 0x2b);  // power settings
    send_command(0x06, 0x17, 0x17, 0x17);        // booster soft start settings

    prepare_wait_busy();
    send_command(0x04);  // power on
    wait_busy();

    send_command(0x00, 0xbf);                    // panel config
    send_command(0x30, 0x3c);                    // pll config
    send_command(0x61, 0x01, 0x90, 0x01, 0x2c);  // resolution
    send_command(0x82, 0x12);                    // vcom_dc setup

    // set partial update window to full screen
    static const uint8_t partial_window_data[] = {0x00, 0x00, 0x01, 399 & 0xff, 0x00, 0x00, 0x01, 200 & 0xff, 0x01};
    send_command(0x90, partial_window_data, sizeof(partial_window_data));
}

}  // namespace

bool display_driver::init() {
    ESP_LOGI(TAG, "initialize display driver");

    spi_bus_config_t spi_bus_config = {.mosi_io_num = SPI_PIN_MOSI,
                                       .miso_io_num = -1,
                                       .sclk_io_num = SPI_PIN_SCLK,
                                       .quadwp_io_num = -1,
                                       .quadhd_io_num = -1,
                                       .data4_io_num = -1,
                                       .data5_io_num = -1,
                                       .data6_io_num = -1,
                                       .data7_io_num = -1,
                                       .max_transfer_sz = SPI_MAX_TRANSFER_SIZE};

    if (spi_bus_initialize(SPI2_HOST, &spi_bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize SPI bus");
        return false;
    };

    spi_device_interface_config_t display_interface_config = {
        .clock_speed_hz = DISPLAY_SPI_SPEED, .spics_io_num = DISPLAY_PIN_CS, .queue_size = 1};

    if (spi_bus_add_device(SPI2_HOST, &display_interface_config, &display_handle) != ESP_OK) {
        ESP_LOGE(TAG, "failed to setup display for SPI");
        return false;
    }

    busyNotificationQueue = xQueueCreate(1, 1);
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
    gpio_isr_handler_add(DISPLAY_PIN_BUSY, busy_isr, nullptr);

    gpio_config_t io_out_config = {.pin_bit_mask = (1 << DISPLAY_PIN_RST) | (1 << DISPLAY_PIN_DC),
                                   .mode = GPIO_MODE_OUTPUT};

    gpio_config_t io_in_config = {.pin_bit_mask = (1 << DISPLAY_PIN_BUSY),
                                  .mode = GPIO_MODE_INPUT,
                                  .pull_up_en = GPIO_PULLUP_ENABLE,
                                  .intr_type = GPIO_INTR_POSEDGE};

    if (gpio_config(&io_out_config) != ESP_OK || gpio_config(&io_in_config) != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure GPIOs");
        return false;
    }

    gpio_set_level(DISPLAY_PIN_RST, 0);
    gpio_set_level(DISPLAY_PIN_DC, 0);

    initialize();
    ESP_LOGI(TAG, "display intialized");

    return true;

    // spi_bus_add_device
}

void display_driver::refresh_display() {
    prepare_wait_busy();
    send_command(0x12);  // refresh display
    wait_busy();
}

void display_driver::set_mode_full() {
    if (current_mode == mode::full) return;

    configure_lut(lut_vcom_full, lut_ww_full, lut_bw_full, lut_wb_full, lut_bb_full);

    send_command(0x92);        // disable partial mode
    send_command(0x50, 0x97);  // white border

    current_mode = mode::full;
}

void display_driver::set_mode_partial() {
    if (current_mode == mode::partial) return;

    configure_lut(lut_vcom_partial, lut_ww_partial, lut_bw_partial, lut_wb_partial, lut_bb_partial);

    send_command(0x91);        // enable partial mode
    send_command(0x50, 0xd7);  // don't update border

    current_mode = mode::partial;
}

display_driver::mode display_driver::get_mode() { return current_mode; }

void display_driver::turn_off() {
    prepare_wait_busy();
    send_command(0x02);
    wait_busy();
}

void display_driver::turn_on() {
    prepare_wait_busy();
    send_command(0x04);
    wait_busy();
}

void display_driver::display_full(const uint8_t* image) { send_command(0x13, image, 300 * 50); }

void display_driver::display_partial(const uint8_t* image_old, const uint8_t* image_new) {
    send_command(0x10, image_old, 300 * 50);
    send_command(0x13, image_new, 300 * 50);
}

void display_driver::display_partial_old(const uint8_t* image_old) { send_command(0x10, image_old, 300 * 50); }

void display_driver::display_partial_new(const uint8_t* image_new) { send_command(0x13, image_new, 300 * 50); }
