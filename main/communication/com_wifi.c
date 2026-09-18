/*
 * com_wifi.c
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Implementasi driver WiFi Station (STA Mode) dan lifecycle event handler.
 */

#include "sdkconfig.h"
#include "com_wifi.h"
#include "main.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_log.h"
#include <string.h>

// Deklarasi forward untuk modul komunikasi yang diaktifkan setelah ada koneksi IP
#include "com_ota.h"
#include "com_mqtt.h"
#include "com_modbus_tcp.h"

static const char *TAG = "COM_WIFI";
static bool s_wifi_connected = false;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi Driver siap, menghubungkan ke AP...");
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        wifi_event_sta_disconnected_t* disconn = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "Koneksi WiFi terputus (Reason: %d)! Mencoba reconnect...", disconn->reason);
        esp_wifi_connect();
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_connected = true;
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, ">>> SUKSES TERHUBUNG KE WIFI! <<<");
        ESP_LOGI(TAG, ">>> IP Address : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, ">>> Netmask    : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, ">>> Gateway    : " IPSTR, IP2STR(&event->ip_info.gw));
        ESP_LOGI(TAG, "==================================================");

        // 1. Otomatis aktifkan Web Server OTA Firmware Update
        com_ota_init();
        ESP_LOGI(TAG, ">>> OTA Web UI Aktif: http://" IPSTR "/update <<<", IP2STR(&event->ip_info.ip));

        // 2. Otomatis aktifkan koneksi MQTT Client ke Broker (EMQX)
        com_mqtt_init(MQTT_BROKER_URI_DEFAULT, MQTT_CLIENT_ID_DEFAULT);

        // 3. Modbus TCP Master - Polling ke Slave perangkat lapangan
        //
        //    Cara mendefinisikan Slave yang ingin di-poll:
        //      { "Nama Device",  "IP Slave",     PORT,  Slave_ID, Start_Addr, Reg_Count, {0}, false }
        //
        //    - Nama Device : label bebas untuk logging di Serial Monitor
        //    - IP Slave    : IP address perangkat Slave di jaringan yang sama
        //    - PORT        : Port TCP (standar Modbus = 502)
        //    - Slave_ID    : Unit ID Modbus Slave (1-247, lihat konfigurasi PLC/device)
        //    - Start_Addr  : Alamat register pertama yang dibaca (0 = register 40001)
        //    - Reg_Count   : Jumlah register yang dibaca
        //
		static modbus_slave_node_t slaves[] = {
		    { "CNC_MILLING_VMC",      "192.168.1.4", 502,  1, 0, 10, {0}, false },
		    { "CNC_LATHE",            "192.168.1.4", 502,  2, 0, 10, {0}, false },
		    { "HYDRAULIC_PRESS_200T", "192.168.1.4", 502,  3, 0, 10, {0}, false },
		    { "WELDING_ROBOT",        "192.168.1.4", 502,  4, 0, 10, {0}, false },
		    { "PAINT_BOOTH_OVEN",     "192.168.1.4", 502,  5, 0, 10, {0}, false },
		    { "WHEEL_LATHE",          "192.168.1.4", 502,  6, 0, 10, {0}, false },
		    { "BOGIE_TEST_RIG",       "192.168.1.4", 502,  7, 0, 10, {0}, false },
		    { "TRACTION_MOTOR_BENCH", "192.168.1.4", 502,  8, 0, 10, {0}, false },
		    { "AIR_COMPRESSOR",       "192.168.1.4", 502,  9, 0, 10, {0}, false },
		    { "COOLING_CHILLER",      "192.168.1.4", 502, 10, 0, 10, {0}, false },
		};
		com_modbus_master_init(slaves, 10);

        ESP_LOGI(TAG, "==================================================");
    }
}
esp_err_t com_wifi_init(const char *ssid, const char *pass) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    const char *target_ssid = (ssid != NULL) ? ssid : WIFI_SSID_DEFAULT;
    const char *target_pass = (pass != NULL) ? pass : WIFI_PASS_DEFAULT;

    wifi_config_t wifi_config = {
        .sta = {
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    strncpy((char *)wifi_config.sta.ssid, target_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, target_pass, sizeof(wifi_config.sta.password));

    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "Driver WiFi dimulai untuk SSID: '%s'", target_ssid);
    return ESP_OK;
}

bool com_wifi_is_connected(void) {
    return s_wifi_connected;
}
