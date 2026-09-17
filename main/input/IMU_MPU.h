#ifndef MAIN_INCLUDE_IMU_MPU_H_
#define MAIN_INCLUDE_IMU_MPU_H_

#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "bsp_pins.h"
#include "main.h"

// Tipe data struct data IMU lengkap (Accel, Gyro, Suhu)
typedef struct {
    float accel_x;          // Akselerasi X (dalam satuan g)
    float accel_y;          // Akselerasi Y (dalam satuan g)
    float accel_z;          // Akselerasi Z (dalam satuan g)
    float gyro_x;           // Kecepatan sudut X (deg/s)
    float gyro_y;           // Kecepatan sudut Y (deg/s)
    float gyro_z;           // Kecepatan sudut Z (deg/s)
    float temp_c;           // Suhu chip (°C)

    int16_t raw_accel_x;
    int16_t raw_accel_y;
    int16_t raw_accel_z;
    int16_t raw_gyro_x;
    int16_t raw_gyro_y;
    int16_t raw_gyro_z;
    int16_t raw_temp;
} imu_data_t;

#define MPU6050_DEFAULT_ADDR    0x68

// Struct konfigurasi modular untuk MPU6050
typedef struct {
    bsp_i2c_pins_t pins;
    uint8_t i2c_addr;
} imu_mpu_config_t;

// Macro helper inisialisasi default menggunakan pin dari main.h
#define IMU_MPU_CONFIG_DEFAULT() ((imu_mpu_config_t){ .pins = I2C_PINS_IMU, .i2c_addr = MPU6050_DEFAULT_ADDR })

// API Driver IMU
esp_err_t imu_mpu_init(const imu_mpu_config_t *config);
esp_err_t imu_mpu_init_pins(bsp_i2c_pins_t pins);
void imu_mpu_update(void);
const imu_data_t* imu_mpu_get_data(void);

#endif /* MAIN_INCLUDE_IMU_MPU_H_ */