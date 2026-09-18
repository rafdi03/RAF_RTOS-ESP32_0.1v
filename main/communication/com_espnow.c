/*
 * com_espnow.c
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Implementasi driver ESP-NOW 2.4 GHz Two-Way terisolasi dengan auto-routing ke Com Hub.
 */

#include "com_espnow.h"
#include "COM.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "COM_ESPNOW";
static const uint8_t s_espnow_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t s_espnow_target_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool s_espnow_initialized = false;

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG, "Paket berhasil terkirim.");
    } else {
        ESP_LOGW(TAG, "Gagal mengirim paket.");
    }
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (recv_info == NULL || data == NULL || len <= 0) {
        return;
    }

    // Simpan MAC pengirim terakhir sebagai default target reply (Two-Way auto-reply)
    memcpy(s_espnow_target_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN);
    com_espnow_on_recv(recv_info->src_addr, data, len);
}

esp_err_t com_espnow_init(const uint8_t *peer_mac, uint8_t channel) {
    if (s_espnow_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Gagal inisialisasi ESP-NOW! Error: %s", esp_err_to_name(ret));
        return ret;
    }

    // Daftarkan fungsi transmisi ESP-NOW ke Central Com Hub
    com_register_tx_handler(COM_IF_ESPNOW, com_espnow_send);

    esp_now_register_send_cb(espnow_send_cb);
    esp_now_register_recv_cb(espnow_recv_cb);

    const uint8_t *target = (peer_mac != NULL) ? peer_mac : s_espnow_broadcast_mac;
    memcpy(s_espnow_target_mac, target, ESP_NOW_ETH_ALEN);

    ret = com_espnow_add_peer(target, channel, false);
    if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGE(TAG, "Gagal mendaftarkan peer default!");
        return ret;
    }

    s_espnow_initialized = true;
    ESP_LOGI(TAG, "Inisialisasi sukses! Target MAC: " MACSTR " | Channel: %u",
             MAC2STR(s_espnow_target_mac), (channel == 0) ? 1 : channel);

    return ESP_OK;
}

esp_err_t com_espnow_add_peer(const uint8_t *peer_mac, uint8_t channel, bool encrypt) {
    if (peer_mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (esp_now_is_peer_exist(peer_mac)) {
        return ESP_OK;
    }

    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, peer_mac, ESP_NOW_ETH_ALEN);
    peer_info.channel = (channel == 0) ? 1 : channel;
    peer_info.ifidx = WIFI_IF_STA;
    peer_info.encrypt = encrypt;

    esp_err_t ret = esp_now_add_peer(&peer_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Gagal menambahkan peer " MACSTR " (Error: %s)",
                 MAC2STR(peer_mac), esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Peer berhasil ditambahkan: " MACSTR " pada channel %u",
             MAC2STR(peer_mac), peer_info.channel);
    return ESP_OK;
}

esp_err_t com_espnow_send(const void *data, size_t len) {
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_espnow_initialized) {
        ESP_LOGW(TAG, "ESP-NOW belum diinisialisasi!");
        return ESP_ERR_INVALID_STATE;
    }

    size_t send_len = (len > ESP_NOW_MAX_DATA_LEN) ? ESP_NOW_MAX_DATA_LEN : len;
    esp_err_t ret = esp_now_send(s_espnow_target_mac, (const uint8_t *)data, send_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Gagal kirim %u bytes ke " MACSTR " (Error: %s)",
                 (unsigned int)send_len, MAC2STR(s_espnow_target_mac), esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

void com_espnow_on_recv(const uint8_t *src_mac, const uint8_t *data, int len) {
    if (data == NULL || len <= 0) return;

    if (src_mac != NULL && !esp_now_is_peer_exist(src_mac)) {
        com_espnow_add_peer(src_mac, 1, false);
    }

    com_push_incoming_request(COM_IF_ESPNOW, data, (size_t)len);
}
