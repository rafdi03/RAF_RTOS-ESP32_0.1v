/*
 * com_modbus_tcp.h
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Universal Modbus TCP Driver Template.
 *             Bisa berfungsi sebagai MASTER (Client) atau SLAVE (Server).
 *
 * ============================================================
 *  CARA PENGGUNAAN CEPAT:
 * ============================================================
 *
 *  [ MODE 1: ESP32 sebagai MASTER (Client) ]
 *    - ESP32 aktif mem-polling data dari perangkat Slave lain di jaringan.
 *    - Contoh: ESP32 <-- membaca sensor --> PLC / PC Simulasi Python.
 *
 *    Langkah:
 *      1. Pastikan MODBUS_MODE = MODBUS_MODE_MASTER (di bawah).
 *      2. Isi tabel s_slave_config[] dengan IP, port, dan slave_id target.
 *      3. Panggil com_modbus_master_init() dari com_wifi.c setelah dapat IP.
 *      4. Gunakan com_modbus_get_slave_data(idx, &data) untuk ambil hasil.
 *
 *  [ MODE 2: ESP32 sebagai SLAVE (Server) ]
 *    - ESP32 pasif menunggu query dari Master (SCADA / PLC / PC).
 *    - Contoh: SCADA di PC <-- membaca --> ESP32 (berisi data sensor IMU).
 *
 *    Langkah:
 *      1. Pastikan MODBUS_MODE = MODBUS_MODE_SLAVE (di bawah).
 *      2. Panggil com_modbus_slave_init() dari com_wifi.c setelah dapat IP.
 *      3. Gunakan com_modbus_set_holding_regs(&data) untuk update nilai sensor.
 *      4. Setiap kali SCADA/Master membaca, nilainya otomatis terbaca.
 *
 * ============================================================
 */

#ifndef MAIN_COMMUNICATION_COM_MODBUS_TCP_H_
#define MAIN_COMMUNICATION_COM_MODBUS_TCP_H_

#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "com_mqtt.h"

/* =========================================================================
 * [KONFIGURASI UTAMA] -- EDIT BAGIAN INI SESUAI KEBUTUHAN ANDA
 * ========================================================================= */

/* Pilih mode operasi:
 *   0 = MASTER (ESP32 aktif membaca dari Slave lain)
 *   1 = SLAVE  (ESP32 pasif, menunggu dibaca oleh Master)
 */
#define MODBUS_MODE_MASTER              0
#define MODBUS_MODE_SLAVE               1
#define MODBUS_MODE                     MODBUS_MODE_MASTER   // <-- UBAH DI SINI

/* Port Modbus TCP (standar industri: 502) */
#define MODBUS_TCP_PORT                 502

/* Jumlah register Holding Register yang digunakan */
#define MODBUS_HOLDING_REG_COUNT        10

/* Timeout koneksi dan terima data per Slave dalam milidetik */
#define MODBUS_CONNECT_TIMEOUT_MS       200

/* Interval antar siklus polling semua slave (ms) */
#define MODBUS_POLL_INTERVAL_MS         1000

/* Jeda antar polling 1 slave ke slave berikutnya (ms) */
#define MODBUS_INTER_SLAVE_DELAY_MS     50

/* Jumlah maksimum Slave yang bisa di-polling (Mode MASTER) */
#define MODBUS_MAX_SLAVES               16


/* =========================================================================
 * STRUKTUR DATA HOLDING REGISTER
 * -- Sesuaikan nama field dengan data sensor Anda
 * ========================================================================= */

 typedef struct {
     uint16_t sensor_1;
     uint16_t sensor_2;
     uint16_t sensor_3;
     uint16_t sensor_4;
     uint16_t sensor_5;
     uint16_t sensor_6;
     uint16_t sensor_7;
     uint16_t sensor_8;
     uint16_t sensor_9;
     uint16_t sensor_10;
 } holding_reg_params_t;


/* =========================================================================
 * STRUKTUR KONFIGURASI SLAVE NODE (Hanya digunakan di Mode MASTER)
 * ========================================================================= */

typedef struct {
    const char *name;               /* Nama device */
    const char *ip;                 /* Alamat IP target Slave */
    uint16_t port;                  /* Port TCP target (biasanya 502) */
    uint8_t slave_id;               /* Unit ID / Slave ID Modbus (1-247) */
    uint16_t start_address;         /* Alamat register pertama yang dibaca (0 = 40001) */
    uint16_t reg_count;             /* Jumlah register yang dibaca */
    holding_reg_params_t data;      /* Hasil pembacaan terakhir (diisi otomatis) */
    bool is_online;                 /* Status koneksi terakhir (diisi otomatis) */
} modbus_slave_node_t;


/* =========================================================================
 * API MODE MASTER
 * ========================================================================= */

/**
 * @brief Inisialisasi Modbus TCP Master Poller.
 *        Membuat FreeRTOS task yang mem-polling semua Slave secara berkala.
 *        Panggil fungsi ini dari com_wifi.c setelah mendapatkan IP.
 *
 * @param slaves     Pointer ke array konfigurasi Slave yang ingin di-poll.
 * @param num_slaves Jumlah slave dalam array (maksimal MODBUS_MAX_SLAVES).
 * @return ESP_OK jika task berhasil dibuat.
 *
 * Contoh penggunaan:
 * ------------------
 *   modbus_slave_node_t my_slaves[] = {
 *       { "DEVICE_01", "192.168.10.101", 502, 1, 0, 5, {0}, false },
 *       { "DEVICE_02", "192.168.10.102", 502, 1, 0, 5, {0}, false },
 *   };
 *   com_modbus_master_init(my_slaves, 2);
 */
esp_err_t com_modbus_master_init(modbus_slave_node_t *slaves, uint8_t num_slaves);

/**
 * @brief Ambil data pembacaan terakhir dari Slave tertentu.
 *
 * @param slave_idx  Indeks slave pada array yang diinputkan ke master_init (mulai dari 0).
 * @param out_data   Pointer struct penampung data hasil pembacaan.
 * @return true jika slave sedang online, false jika offline/error.
 *
 * Contoh penggunaan:
 * ------------------
 *   holding_reg_params_t data;
 *   if (com_modbus_get_slave_data(0, &data)) {
 *       ESP_LOGI(TAG, "Slave 1 - Sensor1: %.1f", data.sensor_1 / 10.0f);
 *   }
 */
bool com_modbus_get_slave_data(uint8_t slave_idx, holding_reg_params_t *out_data);
void send_mqtt_json(void);

/**
 * @brief Cek apakah Slave tertentu sedang online.
 *
 * @param slave_idx Indeks slave.
 * @return true = online, false = offline.
 */
bool com_modbus_is_slave_online(uint8_t slave_idx);


/* =========================================================================
 * API MODE SLAVE
 * ========================================================================= */

/**
 * @brief Inisialisasi Modbus TCP Slave Server.
 *        ESP32 akan membuka port TCP dan menunggu koneksi dari Master (SCADA/PLC).
 *        Panggil fungsi ini dari com_wifi.c setelah mendapatkan IP.
 *
 * @param slave_id Unit ID Modbus (1-247). Default: 1.
 * @return ESP_OK jika server socket berhasil dijalankan.
 *
 * Contoh penggunaan:
 * ------------------
 *   com_modbus_slave_init(1);
 */
esp_err_t com_modbus_slave_init(uint8_t slave_id);

/**
 * @brief Update nilai Holding Register dari kode aplikasi (sensor, IMU, dll).
 *        Nilainya akan otomatis dikembalikan ke Master saat ada query FC 0x03.
 *
 * @param regs Pointer ke struct berisi nilai terbaru sensor.
 *
 * Contoh penggunaan:
 * ------------------
 *   holding_reg_params_t imu_data = {
 *       .sensor_1 = (uint16_t)(roll * 10),
 *       .sensor_2 = (uint16_t)(pitch * 10),
 *   };
 *   com_modbus_set_holding_regs(&imu_data);
 */
void com_modbus_set_holding_regs(const holding_reg_params_t *regs);

/**
 * @brief Baca nilai Holding Register saat ini yang tersimpan di ESP32.
 *
 * @return Salinan struct holding_reg_params_t saat ini.
 */
holding_reg_params_t com_modbus_get_holding_regs(void);


#endif /* MAIN_COMMUNICATION_COM_MODBUS_TCP_H_ */
