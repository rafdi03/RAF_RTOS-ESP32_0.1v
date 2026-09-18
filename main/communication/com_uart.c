/*
 * com_uart.c
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Implementasi driver UART / RS485 Industrial dengan auto-routing ke Com Hub.
 */

#include "com_uart.h"
#include "COM.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "COM_UART";

esp_err_t com_uart_init(bsp_uart_pins_t pins, uint32_t baud_rate) {
    ESP_LOGI(TAG, "Init UART/RS485 TX: %d, RX: %d @ %lu bps", pins.tx, pins.rx, (unsigned long)baud_rate);
    
    // Otomatis daftarkan fungsi TX handler ke Central Com Hub
    com_register_tx_handler(COM_IF_UART, com_uart_send);
    return ESP_OK;
}

esp_err_t com_uart_send(const void *data, size_t len) {
    ESP_LOGI(TAG, "Mengirim %u bytes balasan ke antarmuka UART/RS485", (unsigned int)len);
    // Di sini Anda dapat menambahkan uart_write_bytes() jika menggunakan hardware UART driver
    return ESP_OK;
}

void com_uart_on_rx_bytes(const uint8_t *bytes, size_t len) {
    com_push_incoming_request(COM_IF_UART, bytes, len);
}
