/*
 * com_mqtt.c
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Implementasi driver MQTT client terisolasi dengan auto-routing ke Com Hub.
 */

#include "com_mqtt.h"

static const char *TAG = "COM_MQTT";
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;

static void handle_mqtt_sensor_request(const char *payload, size_t len);

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_mqtt_connected = true;
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, ">>> SUKSES TERHUBUNG KE BROKER MQTT (EMQX)! <<<");
        ESP_LOGI(TAG, ">>> Subscribed ke Topik Request : %s", MQTT_TOPIC_REQ_DEFAULT);
        ESP_LOGI(TAG, ">>> Siap publish ke Topik Response : %s", MQTT_TOPIC_RESP_DEFAULT);
        ESP_LOGI(TAG, "==================================================");
        esp_mqtt_client_subscribe(s_mqtt_client, MQTT_TOPIC_REQ_DEFAULT, 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_mqtt_connected = false;
        ESP_LOGW(TAG, "Terputus dari Broker MQTT! Mencoba menghubungkan kembali...");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Berhasil subscribe ke request topic (msg_id=%d)", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
	ESP_LOGI(TAG, "Request masuk di topik '%.*s' (%d bytes): %.*s",
	            event->topic_len, event->topic,
	            event->data_len, event->data_len, event->data);

	   /* Cek apakah ini request spesifik baca sensor */
	   if (event->data_len > 0 &&
	       strstr((char *)event->data, "\"device\"") != NULL &&
	       strstr((char *)event->data, "\"sensor\"") != NULL) {

	       handle_mqtt_sensor_request(event->data, (size_t)event->data_len);
	   } else {
	       /* Request lama ({"cmd":"ping"}, dll) → teruskan ke Com Hub seperti biasa */
	       com_push_incoming_request(COM_IF_MQTT, event->data, (size_t)event->data_len);
	   }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Terjadi error pada client MQTT!");
        break;
    default:
        break;
    }
}

esp_err_t com_mqtt_init(const char *broker_uri, const char *client_id) {
    if (s_mqtt_client != NULL) {
        return ESP_OK; // Idempoten: sudah diinisialisasi
    }

    const char *uri = (broker_uri != NULL) ? broker_uri : MQTT_BROKER_URI_DEFAULT;
    const char *cid = (client_id != NULL) ? client_id : MQTT_CLIENT_ID_DEFAULT;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .credentials.client_id = cid,
		.session.keepalive = 60,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Gagal membuat instance MQTT client!");
        return ESP_FAIL;
    }

    // Daftarkan fungsi transmisi ke Com Hub agar Com Hub bisa membalas otomatis lewat MQTT
    com_register_tx_handler(COM_IF_MQTT, com_mqtt_publish);

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t ret = esp_mqtt_client_start(s_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Gagal memulai MQTT client (Error: %s)!", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Client started. Menghubungkan ke Broker: %s (Client: %s)...", uri, cid);
    return ESP_OK;
}

esp_err_t com_mqtt_publish(const void *data, size_t len) {
    if (s_mqtt_client == NULL || !s_mqtt_connected) {
        ESP_LOGW(TAG, "Client belum siap / belum terhubung ke broker!");
        return ESP_ERR_INVALID_STATE;
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_RESP_DEFAULT, (const char *)data, len, 0, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Gagal publish ke '%s'!", MQTT_TOPIC_RESP_DEFAULT);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sukses publish (%u bytes) ke '%s'", (unsigned int)len, MQTT_TOPIC_RESP_DEFAULT);
    return ESP_OK;
}

void send_mqtt_json(void) {
    if (!com_mqtt_is_connected()) return;

    holding_reg_params_t slave_data;
    static char json_buffer[2048];
    int offset = snprintf(json_buffer, sizeof(json_buffer),
                          "{\"ts\":%lu,\"src\":\"periodic\",\"devices\":[",
                          (unsigned long)esp_timer_get_time());

    uint8_t online_count = 0;

    for (uint8_t i = 0; i < 10; i++) {
        if (com_modbus_get_slave_data(i, &slave_data)) {
            if (online_count > 0) {
                offset += snprintf(json_buffer + offset,
                                   sizeof(json_buffer) - offset, ",");
            }
            offset += snprintf(json_buffer + offset,
                               sizeof(json_buffer) - offset,
                     "{\"id\":\"DEVICE_%02d\","
                     "\"s1\":%d,\"s2\":%d,\"s3\":%d,\"s4\":%d,\"s5\":%d,"
                     "\"s6\":%d,\"s7\":%d,\"s8\":%d,\"s9\":%d,\"s10\":%d}",
                     i + 1,
                     slave_data.sensor_1,  slave_data.sensor_2,
                     slave_data.sensor_3,  slave_data.sensor_4,
                     slave_data.sensor_5,  slave_data.sensor_6,
                     slave_data.sensor_7,  slave_data.sensor_8,
                     slave_data.sensor_9,  slave_data.sensor_10);
            online_count++;
        }
    }

    /* Tutup array + sertakan status node global */
    offset += snprintf(json_buffer + offset, sizeof(json_buffer) - offset,
                       "],\"online\":%d}", online_count);

    esp_err_t err = com_mqtt_publish(json_buffer, strlen(json_buffer));
    if (err != ESP_OK) {
        ESP_LOGE("COM_MQTT", "Gagal periodik publish: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI("COM_MQTT", "Periodic publish OK (%d device online, %d bytes)",
                 online_count, offset);
    }
}

/* =========================================================================
 * Handler khusus: request baca sensor individual
 * Format: {"device":N,"sensor":M}
 * ========================================================================= */
static void handle_mqtt_sensor_request(const char *payload, size_t len) {
    if (!s_mqtt_connected) return;

    char req[128] = {0};
    size_t copy = (len < sizeof(req) - 1) ? len : sizeof(req) - 1;
    memcpy(req, payload, copy);

    int device = -1;
    int sensor = -1;

    const char *p_dev = strstr(req, "\"device\"");
    const char *p_sen = strstr(req, "\"sensor\"");
    if (p_dev) { const char *c = strchr(p_dev, ':'); if (c) device = atoi(c + 1); }
    if (p_sen) { const char *c = strchr(p_sen, ':'); if (c) sensor = atoi(c + 1); }

    char resp[256];
    int off = 0;

    /* Validasi */
    if (device < 1 || device > 10) {
        off = snprintf(resp, sizeof(resp),
                       "{\"src\":\"request\",\"err\":\"device range 1-10\"}");
        com_mqtt_publish(resp, strlen(resp));
        return;
    }
    if (sensor < 1 || sensor > 10) {
        off = snprintf(resp, sizeof(resp),
                       "{\"src\":\"request\",\"err\":\"sensor range 1-10\"}");
        com_mqtt_publish(resp, strlen(resp));
        return;
    }

    holding_reg_params_t data;
    if (!com_modbus_get_slave_data((uint8_t)(device - 1), &data)) {
        off = snprintf(resp, sizeof(resp),
                       "{\"src\":\"request\",\"device\":%d,\"online\":0,"
                       "\"err\":\"device offline\"}", device);
        com_mqtt_publish(resp, strlen(resp));
        ESP_LOGW(TAG, "Device %d offline", device);
        return;
    }

    /* Akses field sensor_1..sensor_10 by index */
    uint16_t *raw = (uint16_t *)&data;
    uint16_t val  = raw[sensor - 1];

    off = snprintf(resp, sizeof(resp),
                   "{\"src\":\"request\",\"device\":%d,\"sensor\":%d,"
                   "\"value\":%d,\"online\":1}",
                   device, sensor, val);

    ESP_LOGI(TAG, "Sensor response: %s", resp);
    com_mqtt_publish(resp, strlen(resp));
}

bool com_mqtt_is_connected(void) {
    return s_mqtt_connected;
}
