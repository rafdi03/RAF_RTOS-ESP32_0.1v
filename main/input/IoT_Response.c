/*
 * IoT_Response.c
 *
 *  Created on: 9 Sept 2026
 *      Author: Rafdi
 */

#include "IoT_Response.h"

static const char *TAG = "IOT_RESP";

/**
 * @brief Callback penyedia data payload biner (Hardware protocol / LoRa / Modbus / UART)
 */
static bool iot_binary_cmd_handler(uint8_t cmd_code, const uint8_t *in_payload, uint8_t in_len, uint8_t *out_payload, uint8_t *out_len) {
    if (cmd_code == CMD_REQ_IMU || cmd_code == CMD_REQ_ALL_SENSORS) {
        const imu_data_t *imu = imu_mpu_get_data();
        *out_len = sizeof(float) * 3;
        memcpy(&out_payload[0], &imu->accel_x, sizeof(float));
        memcpy(&out_payload[4], &imu->accel_y, sizeof(float));
        memcpy(&out_payload[8], &imu->accel_z, sizeof(float));
        return true;
    }
    return false;
}

/**
 * @brief Callback pemroses request string JSON dari Website / MQTT / Dashboard
 *        Menyaring permintaan sensor tertentu secara on-demand.
 */
static bool iot_json_request_handler(const char *json_req, char *json_resp, size_t max_resp_len) {
    const imu_data_t *imu = imu_mpu_get_data();

    // Hitung orientasi Roll & Pitch dari proyeksi akselerometer (derajat)
    float roll = atan2f(imu->accel_y, imu->accel_z) * 57.29578f;
    float pitch = atan2f(-imu->accel_x, sqrtf(imu->accel_y * imu->accel_y + imu->accel_z * imu->accel_z)) * 57.29578f;

    // 1. Request khusus IMU / Akselerasi
    if (strstr(json_req, "\"req\":\"imu\"") || strstr(json_req, "\"req\":\"accel\"")) {
        snprintf(json_resp, max_resp_len,
                 "{\"status\":\"OK\",\"type\":\"imu\",\"accel_x\":%.2f,\"accel_y\":%.2f,\"accel_z\":%.2f,\"gyro_x\":%.2f,\"gyro_y\":%.2f,\"gyro_z\":%.2f,\"roll\":%.2f,\"pitch\":%.2f,\"yaw\":0.00}",
                 imu->accel_x, imu->accel_y, imu->accel_z,
                 imu->gyro_x, imu->gyro_y, imu->gyro_z,
                 roll, pitch);
        return true;
    }
    // 2. Request khusus Suhu (Temp)
    else if (strstr(json_req, "\"req\":\"temp\"") || strstr(json_req, "\"req\":\"suhu\"")) {
        snprintf(json_resp, max_resp_len,
                 "{\"status\":\"OK\",\"type\":\"temp\",\"temp_c\":%.1f,\"unit\":\"C\"}",
                 imu->temp_c);
        return true;
    }
    // 3. Request SEMUA sensor aktif (All Sensors - Cocok untuk 3D Web Dashboard)
    else if (strstr(json_req, "\"req\":\"all\"")) {
        snprintf(json_resp, max_resp_len,
                 "{\"status\":\"OK\",\"type\":\"all\",\"accel_x\":%.2f,\"accel_y\":%.2f,\"accel_z\":%.2f,\"gyro_x\":%.2f,\"gyro_y\":%.2f,\"gyro_z\":%.2f,\"temp_c\":%.1f,\"roll\":%.2f,\"pitch\":%.2f,\"yaw\":0.00}",
                 imu->accel_x, imu->accel_y, imu->accel_z,
                 imu->gyro_x, imu->gyro_y, imu->gyro_z,
                 imu->temp_c, roll, pitch);
        return true;
    }
    // 4. Request Ping / Heartbeat
    else if (strstr(json_req, "\"req\":\"ping\"")) {
        snprintf(json_resp, max_resp_len, 
                 "{\"status\":\"OK\",\"msg\":\"pong\",\"uptime\":%lu}",
                 (unsigned long)(esp_timer_get_time() / 1000000ULL));
        return true;
    }

    return false;
}

void iot_response_init(void) {
    com_register_cmd_handler(iot_binary_cmd_handler);
    com_register_json_handler(iot_json_request_handler);
    ESP_LOGI(TAG, "IoT Response Handlers (JSON & Biner) berhasil didaftarkan ke Central Com Hub.");
}
