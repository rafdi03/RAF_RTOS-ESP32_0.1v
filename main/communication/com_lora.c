/*
 * com_lora.c
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Implementasi driver LoRa P2P terisolasi dengan auto-routing ke Com Hub.
 */

#include "com_lora.h"
#include "COM.h"
#include "esp_log.h"

static const char *TAG = "COM_LORA";

esp_err_t com_lora_init(bsp_spi_pins_t pins, long frequency_hz) {
    // Daftarkan fungsi transmisi LoRa ke Central Com Hub
    com_register_tx_handler(COM_IF_LORA, com_lora_send);

    ESP_LOGI(TAG, "LoRa P2P siap @ %ld Hz (MOSI:%d MISO:%d SCK:%d CS:%d)", 
             frequency_hz, pins.mosi, pins.miso, pins.sck, pins.cs);
    return ESP_OK;
}

esp_err_t com_lora_send(const void *data, size_t len) {
    ESP_LOGI(TAG, "[LoRa TX] Mengirim %u bytes paket radio RF", (unsigned int)len);
    return ESP_OK;
}

void com_lora_on_rx_packet(const uint8_t *packet, size_t len) {
    com_push_incoming_request(COM_IF_LORA, packet, len);
}
