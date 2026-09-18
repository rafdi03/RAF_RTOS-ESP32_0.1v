/*
 * com_ota.c
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Implementasi Web Server lokal untuk OTA Firmware Update (Browser-based).
 */

#include "com_ota.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "COM_OTA";
static httpd_handle_t s_httpd_server = NULL;

#define OTA_BUFF_SIZE 1024

/* =========================================================================
 * HALAMAN WEB UI OTA (HTML5 + CSS Modern)
 * ========================================================================= */

static const char *s_ota_html_page = 
"<!DOCTYPE html>"
"<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>ESP32 Firmware OTA Update</title>"
"<style>"
"body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0f172a; color: #f8fafc; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px; }"
".card { background: #1e293b; border-radius: 16px; padding: 32px; width: 100%; max-width: 480px; box-shadow: 0 10px 25px rgba(0,0,0,0.5); border: 1px solid #334155; }"
"h2 { margin-top: 0; color: #38bdf8; font-size: 24px; text-align: center; }"
".info { background: #0f172a; border-radius: 8px; padding: 14px; margin-bottom: 24px; font-size: 13px; color: #94a3b8; }"
".info div { margin-bottom: 6px; }"
".info div:last-child { margin-bottom: 0; }"
".info span { color: #f1f5f9; font-weight: 600; }"
".file-drop { border: 2px dashed #475569; border-radius: 12px; padding: 24px; text-align: center; margin-bottom: 20px; cursor: pointer; transition: 0.2s; }"
".file-drop:hover { border-color: #38bdf8; background: rgba(56, 189, 248, 0.05); }"
"input[type=file] { display: none; }"
".btn { background: #0284c7; color: white; border: none; padding: 12px 20px; font-size: 15px; font-weight: 600; border-radius: 8px; width: 100%; cursor: pointer; transition: 0.2s; }"
".btn:hover { background: #0369a1; }"
".btn:disabled { background: #475569; cursor: not-allowed; }"
".progress-container { display: none; margin-top: 20px; }"
".progress-bar { width: 100%; height: 10px; background: #334155; border-radius: 5px; overflow: hidden; }"
".progress-fill { width: 0%; height: 100%; background: #38bdf8; transition: width 0.2s; }"
".status { margin-top: 10px; font-size: 14px; text-align: center; color: #cbd5e1; }"
"</style></head><body>"
"<div class='card'>"
"<h2>&#9889; ESP32 OTA Firmware Update</h2>"
"<div class='info'>"
"<div>System Target: <span>ESP32 Dual-Core (FreeRTOS)</span></div>"
"<div>Update Method: <span>Browser Direct Stream</span></div>"
"<div>Supported File: <span>hello_world.bin</span></div>"
"</div>"
"<div class='file-drop' onclick='document.getElementById(\"file\").click()'>"
"<div id='file-label'>&#128193; Klik untuk memilih file firmware (.bin)</div>"
"<input type='file' id='file' accept='.bin' onchange='onFileSelected()'>"
"</div>"
"<button id='upload-btn' class='btn' onclick='uploadFirmware()' disabled>Upload & Flash Firmware</button>"
"<div class='progress-container' id='prg-box'>"
"<div class='progress-bar'><div class='progress-fill' id='prg-fill'></div></div>"
"<div class='status' id='status-text'>Mengunggah: 0%</div>"
"</div>"
"</div>"
"<script>"
"var selectedFile = null;"
"function onFileSelected() {"
"  var fi = document.getElementById('file');"
"  if (fi.files.length > 0) {"
"    selectedFile = fi.files[0];"
"    document.getElementById('file-label').innerHTML = '&#128196; ' + selectedFile.name + ' (' + Math.round(selectedFile.size/1024) + ' KB)';"
"    document.getElementById('upload-btn').disabled = false;"
"  }"
"}"
"function uploadFirmware() {"
"  if (!selectedFile) return;"
"  var btn = document.getElementById('upload-btn');"
"  btn.disabled = true;"
"  document.getElementById('prg-box').style.display = 'block';"
"  var xhr = new XMLHttpRequest();"
"  xhr.open('POST', '/update', true);"
"  xhr.upload.onprogress = function(e) {"
"    if (e.lengthComputable) {"
"      var pct = Math.round((e.loaded / e.total) * 100);"
"      document.getElementById('prg-fill').style.width = pct + '%';"
"      document.getElementById('status-text').innerText = 'Mengunggah & Flashing: ' + pct + '%';"
"    }"
"  };"
"  xhr.onload = function() {"
"    if (xhr.status == 200) {"
"      document.getElementById('status-text').innerHTML = '<span style=\"color:#4ade80;\">&#10004; Flashing Sukses! Me-reboot ESP32 dalam 5 detik...</span>';"
"      setTimeout(function(){ location.reload(); }, 6000);"
"    } else {"
"      document.getElementById('status-text').innerHTML = '<span style=\"color:#f87171;\">&#10008; Gagal update: ' + xhr.responseText + '</span>';"
"      btn.disabled = false;"
"    }"
"  };"
"  xhr.onerror = function() {"
"    document.getElementById('status-text').innerHTML = '<span style=\"color:#f87171;\">&#10008; Error jaringan saat upload.</span>';"
"    btn.disabled = false;"
"  };"
"  xhr.send(selectedFile);"
"}"
"</script></body></html>";

/* =========================================================================
 * HTTP REQUEST HANDLERS
 * ========================================================================= */

// Handler GET /update (Sajikan Halaman Web UI)
static esp_err_t ota_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, s_ota_html_page, HTTPD_RESP_USE_STRLEN);
}

// Handler GET / (Redirect ke /update)
static esp_err_t index_get_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/update");
    return httpd_resp_send(req, NULL, 0);
}

// Task delayed reboot
static void delayed_restart_task(void *pvParameter) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP_LOGI(TAG, "Rebooting ke partisi firmware baru sekarang...");
    esp_restart();
}

// Handler POST /update (Streaming Binary Chunk & Menulis ke Flash OTA)
static esp_err_t ota_post_handler(httpd_req_t *req) {
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Gagal menemukan partisi target OTA!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Target OTA partition not found");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Memulai proses OTA pada partisi '%s' @ 0x%08lX (Ukuran: %lu KB)",
             update_partition->label,
             (unsigned long)update_partition->address,
             (unsigned long)(update_partition->size / 1024));

    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin gagal (Error: %s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Begin failed");
        return ESP_FAIL;
    }

    char *ota_buf = malloc(OTA_BUFF_SIZE);
    if (ota_buf == NULL) {
        esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    int remaining = req->content_len;
    int received = 0;
    bool is_first_chunk = true;

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, ota_buf, (remaining > OTA_BUFF_SIZE) ? OTA_BUFF_SIZE : remaining);
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue; // Retry jika socket timeout sesaat
            }
            ESP_LOGE(TAG, "Socket error saat menerima data OTA");
            free(ota_buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Socket receive error");
            return ESP_FAIL;
        }

        if (is_first_chunk) {
            is_first_chunk = false;
            // Validasi byte pertama biner ESP32 (Magic Byte 0xE9)
            if (ota_buf[0] != 0xE9) {
                ESP_LOGE(TAG, "File yang diupload bukan file firmware ESP32 valid (.bin)!");
                free(ota_buf);
                esp_ota_abort(ota_handle);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid binary image");
                return ESP_FAIL;
            }
        }

        err = esp_ota_write(ota_handle, (const void *)ota_buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write gagal (Error: %s)", esp_err_to_name(err));
            free(ota_buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Write failed");
            return ESP_FAIL;
        }

        received += recv_len;
        remaining -= recv_len;
    }

    free(ota_buf);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end gagal (Verifikasi image error: %s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA End/Verify failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition gagal (Error: %s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, ">>> SUKSES BESAR! OTA Update selesai (%d bytes). Me-reboot... <<<", received);
    httpd_resp_sendstr(req, "OTA Update Sukses! Rebooting...");

    // Trigger reboot asinkron
    xTaskCreate(delayed_restart_task, "delayed_restart", 2048, NULL, 5, NULL);
    return ESP_OK;
}

/* =========================================================================
 * REGISTRASI HTTP SERVER
 * ========================================================================= */

esp_err_t com_ota_init(void) {
    if (s_httpd_server != NULL) {
        return ESP_OK; 
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&s_httpd_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Gagal memulai HTTP Web Server OTA (Error: %s)", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t uri_get_index = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(s_httpd_server, &uri_get_index);

    httpd_uri_t uri_get_update = {
        .uri       = "/update",
        .method    = HTTP_GET,
        .handler   = ota_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(s_httpd_server, &uri_get_update);

    httpd_uri_t uri_post_update = {
        .uri       = "/update",
        .method    = HTTP_POST,
        .handler   = ota_post_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(s_httpd_server, &uri_post_update);

    ESP_LOGI(TAG, "HTTP Server OTA Update aktif. Akses melalui: http://<IP_ESP32>/update");
    return ESP_OK;
}

void com_ota_stop(void) {
    if (s_httpd_server != NULL) {
        httpd_stop(s_httpd_server);
        s_httpd_server = NULL;
        ESP_LOGI(TAG, "HTTP Server OTA Update dihentikan.");
    }
}
