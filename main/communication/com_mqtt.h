/*
 * com_mqtt.h
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Modul komunikasi MQTT Pub/Sub Client.
 */

#ifndef MAIN_COMMUNICATION_COM_MQTT_H_
#define MAIN_COMMUNICATION_COM_MQTT_H_

#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>
#include "esp_timer.h"
#include "com_modbus_tcp.h"
#include <string.h>
#include "COM.h"
#include "main.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "COM.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdlib.h> 

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inisialisasi client MQTT dan menghubungkan ke broker secara asinkron.
 *        Otomatis mendaftarkan fungsi TX handler ke Central Com Hub.
 * @param broker_uri Alamat URI broker MQTT (jika NULL, gunakan MQTT_BROKER_URI_DEFAULT).
 * @param client_id Nama identitas client MQTT (jika NULL, gunakan MQTT_CLIENT_ID_DEFAULT).
 * @return ESP_OK jika client berhasil dibuat dan dimulai.
 */
esp_err_t com_mqtt_init(const char *broker_uri, const char *client_id);

/**
 * @brief Mempublikasikan (Publish) data balasan ke topik response MQTT broker.
 * @param data Pointer ke payload data yang akan dipublikasikan.
 * @param len Ukuran data payload dalam bytes.
 * @return ESP_OK jika publish berhasil, atau error code jika gagal.
 */
esp_err_t com_mqtt_publish(const void *data, size_t len);

/**
 * @brief Memeriksa apakah client MQTT saat ini terhubung ke broker.
 * @return true jika terhubung, false jika tidak.
 */
bool com_mqtt_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_COMMUNICATION_COM_MQTT_H_ */
