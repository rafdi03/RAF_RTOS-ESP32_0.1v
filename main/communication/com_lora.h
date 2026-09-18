/*
 * com_lora.h
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Modul komunikasi radio LoRa P2P (SX1276 / SX1278) via SPI.
 */

#ifndef MAIN_COMMUNICATION_COM_LORA_H_
#define MAIN_COMMUNICATION_COM_LORA_H_

#pragma once
#include "esp_err.h"
#include "bsp_pins.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inisialisasi modul radio LoRa P2P via SPI.
 *        Otomatis mendaftarkan fungsi TX handler ke Central Com Hub.
 * @param pins Pin konfigurasi SPI (MOSI, MISO, SCK, CS).
 * @param frequency_hz Frekuensi RF dalam Hz (misal 915000000 / 433000000).
 * @return ESP_OK jika inisialisasi berhasil.
 */
esp_err_t com_lora_init(bsp_spi_pins_t pins, long frequency_hz);

/**
 * @brief Mengirimkan paket data biner melalui radio LoRa RF (TX Handler).
 * @param data Pointer ke payload paket.
 * @param len Ukuran data paket dalam bytes.
 * @return ESP_OK jika paket berhasil dikirim.
 */
esp_err_t com_lora_send(const void *data, size_t len);

/**
 * @brief Callback event penampung paket radio masuk dari LoRa Receiver ke Com Hub.
 * @param packet Buffer data yang diterima.
 * @param len Panjang data dalam bytes.
 */
void com_lora_on_rx_packet(const uint8_t *packet, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_COMMUNICATION_COM_LORA_H_ */
