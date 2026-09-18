/*
 * com_can.h
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Modul komunikasi CAN Bus / TWAI Controller (Automotive & Industrial).
 */

#ifndef MAIN_COMMUNICATION_COM_CAN_H_
#define MAIN_COMMUNICATION_COM_CAN_H_

#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inisialisasi kontroler CAN Bus / TWAI hardware ESP32.
 *        Otomatis mendaftarkan fungsi TX handler ke Central Com Hub.
 * @param tx_pin Nomor pin GPIO untuk jalur Transmit (CAN TX).
 * @param rx_pin Nomor pin GPIO untuk jalur Receive (CAN RX).
 * @param baud_rate_kbps Kecepatan bus CAN dalam kbps (contoh: 125, 250, 500, 1000 kbps).
 * @return ESP_OK jika inisialisasi driver TWAI berhasil.
 */
esp_err_t com_can_init(int tx_pin, int rx_pin, uint32_t baud_rate_kbps);

/**
 * @brief Mengirimkan frame paket CAN Bus ke jaringan.
 * @param data Pointer ke isi payload frame data CAN (maksimal 8 bytes).
 * @param len Panjang payload frame data dalam bytes.
 * @return ESP_OK jika transmisi frame berhasil di-enqueue.
 */
esp_err_t com_can_send_frame(const void *data, size_t len);

/**
 * @brief Melakukan polling non-blocking frame masuk dari antrean hardware CAN Bus / TWAI.
 *        Wajib dipanggil berkala (misal di job_1ms).
 */
void com_can_rx_poll(void);

/**
 * @brief Callback penerimaan frame CAN Bus dari kontroler TWAI ke Com Hub.
 * @param can_id Identifier unik frame CAN.
 * @param data Pointer ke isi array payload frame data.
 * @param dlc Data Length Code (0 - 8 bytes).
 */
void com_can_on_frame_received(uint32_t can_id, const uint8_t *data, uint8_t dlc);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_COMMUNICATION_COM_CAN_H_ */
