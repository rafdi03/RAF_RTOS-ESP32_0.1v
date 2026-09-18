/*
 * com_can.c
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Implementasi driver CAN Bus / TWAI terisolasi dengan auto-routing ke Com Hub.
 */

#include "com_can.h"
#include "COM.h"
#include "esp_log.h"
#include <string.h>
#include "driver/gpio.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include "driver/twai.h"
#pragma GCC diagnostic pop

static const char *TAG = "COM_CAN";
static bool s_can_initialized = false;

esp_err_t com_can_init(int tx_pin, int rx_pin, uint32_t baud_rate_kbps) {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)tx_pin, 
        (gpio_num_t)rx_pin, 
        TWAI_MODE_NORMAL
    );

    twai_timing_config_t t_config;
    switch (baud_rate_kbps) {
        case 125:  t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_125KBITS(); break;
        case 250:  t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS(); break;
        case 1000: t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS(); break;
        case 500:
        default:   t_config = (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS(); break;
    }

    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Gagal menginstall CAN/TWAI driver!");
        return ret;
    }

    ret = twai_start();
    if (ret == ESP_OK) {
        s_can_initialized = true;
        // Daftarkan fungsi transmisi CAN Bus ke Central Com Hub
        com_register_tx_handler(COM_IF_CAN, com_can_send_frame);
        ESP_LOGI(TAG, "CAN Bus (TWAI) Aktif @ %lu kbps pada TX:%d, RX:%d", (unsigned long)baud_rate_kbps, tx_pin, rx_pin);
    }
    return ret;
}

esp_err_t com_can_send_frame(const void *data, size_t len) {
    if (!s_can_initialized) return ESP_ERR_INVALID_STATE;
    if (data == NULL || len == 0) return ESP_ERR_INVALID_ARG;

    twai_message_t tx_msg = {
        .identifier = 0x123,           
        .extd = 0,                 
        .data_length_code = (len > 8) ? 8 : len,
    };
    memcpy(tx_msg.data, data, tx_msg.data_length_code);
    return twai_transmit(&tx_msg, pdMS_TO_TICKS(10));
}

void com_can_on_frame_received(uint32_t can_id, const uint8_t *data, uint8_t dlc) {
    com_inbound_req_t req = {
        .preamble = COMM_PACKET_PREAMBLE,
        .cmd_code = (uint8_t)(can_id & 0xFF),
        .payload_len = (dlc > 32) ? 32 : dlc,
        .iface_source = (uint8_t)COM_IF_CAN
    };
    if (data && dlc > 0) {
        memcpy(req.payload, data, req.payload_len);
    }
    req.crc16 = comm_crc16(&req, offsetof(com_inbound_req_t, crc16));

    com_push_incoming_request(COM_IF_CAN, &req, sizeof(req));
}

void com_can_rx_poll(void) {
    if (!s_can_initialized) return;
    twai_message_t rx_msg;
    while (twai_receive(&rx_msg, 0) == ESP_OK) {
        com_can_on_frame_received(rx_msg.identifier, rx_msg.data, rx_msg.data_length_code);
    }
}
