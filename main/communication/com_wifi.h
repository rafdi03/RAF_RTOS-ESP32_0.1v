/*
 * com_wifi.h
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Modul komunikasi WiFi Station (STA Mode) dan event loop.
 */

#ifndef MAIN_COMMUNICATION_COM_WIFI_H_
#define MAIN_COMMUNICATION_COM_WIFI_H_

#pragma once
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inisialisasi stack WiFi Station (STA Mode) dan auto-reconnect.
 * @param ssid Nama WiFi Access Point (jika NULL, gunakan WIFI_SSID_DEFAULT dari main.h).
 * @param pass Password WiFi (jika NULL, gunakan WIFI_PASS_DEFAULT dari main.h).
 * @return ESP_OK jika inisialisasi berhasil dimulai.
 */
esp_err_t com_wifi_init(const char *ssid, const char *pass);

/**
 * @brief Memeriksa status koneksi WiFi ke Access Point.
 * @return true jika sudah tersambung dan mendapatkan IP, false jika belum.
 */
bool com_wifi_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_COMMUNICATION_COM_WIFI_H_ */
