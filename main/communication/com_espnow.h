/*
 * com_espnow.h
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Modul komunikasi ESP-NOW 2.4 GHz Two-Way Peer-to-Peer & Broadcast.
 */

#ifndef MAIN_COMMUNICATION_COM_ESPNOW_H_
#define MAIN_COMMUNICATION_COM_ESPNOW_H_

#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inisialisasi stack ESP-NOW 2-Way communication (Master/Slave).
 *        Otomatis mendaftarkan fungsi TX handler ke Central Com Hub.
 * @param peer_mac Alamat MAC 6-byte target (atau NULL untuk broadcast FF:FF:FF:FF:FF:FF).
 * @param channel Channel WiFi (1 - 13, default 1).
 * @return ESP_OK jika inisialisasi berhasil.
 */
esp_err_t com_espnow_init(const uint8_t *peer_mac, uint8_t channel);

/**
 * @brief Menambahkan peer baru ke daftar komunikasi ESP-NOW secara dinamis.
 * @param peer_mac Alamat 6-byte MAC target.
 * @param channel Channel WiFi target.
 * @param encrypt Status enkripsi paket (true / false).
 * @return ESP_OK jika peer berhasil ditambahkan.
 */
esp_err_t com_espnow_add_peer(const uint8_t *peer_mac, uint8_t channel, bool encrypt);

/**
 * @brief Mengirimkan paket data biner melalui protokol nirkabel ESP-NOW (TX Handler).
 * @param data Pointer ke data paket payload.
 * @param len Ukuran data paket dalam bytes (maksimal 250 bytes).
 * @return ESP_OK jika paket berhasil dikirim.
 */
esp_err_t com_espnow_send(const void *data, size_t len);

/**
 * @brief Callback penerimaan data masuk ESP-NOW dari peer/master ke Com Hub.
 * @param src_mac Pointer ke alamat MAC pengirim paket.
 * @param data Pointer ke payload data.
 * @param len Panjang data payload dalam bytes.
 */
void com_espnow_on_recv(const uint8_t *src_mac, const uint8_t *data, int len);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_COMMUNICATION_COM_ESPNOW_H_ */
