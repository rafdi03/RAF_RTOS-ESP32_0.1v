/*
 * com_ota.h
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Modul Over-The-Air (OTA) Firmware Update berbasis Web Server lokal.
 *             Memungkinkan flashing file .bin langsung via browser http://<IP>/update.
 */

#ifndef MAIN_COMMUNICATION_COM_OTA_H_
#define MAIN_COMMUNICATION_COM_OTA_H_

#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inisialisasi Web Server internal untuk halaman OTA Firmware Update.
 * @return ESP_OK jika Web Server berhasil dijalankan, atau error code jika gagal.
 */
esp_err_t com_ota_init(void);

/**
 * @brief Menghentikan Web Server OTA Firmware Update.
 */
void com_ota_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_COMMUNICATION_COM_OTA_H_ */
