/*
 * task_registry.c
 *
 *  Created on: 18 Sept 2026
 *      Author: Rafdi
 */


 #include "task_registry.h"
 #include "task.h"
 #include "main.h"
 #include "COM.h"
 #include "ringbuff_com.h"
 #include "IMU_MPU.h"
 #include "IoT_Response.h"
 #include "com_can.h"
 #include "com_modbus_tcp.h"
 #include "com_wifi.h"
 #include "esp_log.h"
 #include <stdint.h>

 static const char *TAG = "TASK_REGISTRY";

 static esp_err_t init_application(void) {
     esp_err_t err = ringbuf_com_init(RINGBUF_COMM_DEFAULT_SIZE);
     if (err != ESP_OK) return err;

     err = com_init();
     if (err != ESP_OK) return err;

     err = imu_mpu_init(NULL);
     if (err != ESP_OK) return err;

     iot_response_init();
     return com_wifi_init(WIFI_SSID_DEFAULT, WIFI_PASS_DEFAULT);
 }

 static void job_com_dispatch(void *arg) {
     (void)arg;
     com_update_1ms();
     com_can_rx_poll();
 }

 static void job_system_diagnostics(void *arg) {
     (void)arg;
     ringbuf_com_print_stats();
     rt_scheduler_print_stats();
	 
	 static char cpu_buf[1024];
	     vTaskGetRunTimeStats(cpu_buf);
	     ESP_LOGI("CPU", "\n%s", cpu_buf);

     static uint16_t tick = 0;
     if (++tick >= 10) {
         tick = 0;
         send_mqtt_json();
     }
 }

 static void job_timing_test(void *arg) {
     (void)arg;
     static uint32_t counter = 0;
     counter++;
     if (counter % 10000 == 0) {
         // Print setiap 10000 eksekusi (10 detik kalau period 1 ms)
         ESP_LOGI("TIMING", "Counter: %lu (harusnya ~10000 per 10 detik)", 
                  (unsigned long)counter);
     }
 }
 // =========================================================================
 // CONFIG TABLE
 // =========================================================================
 // Format setiap entry:
 //
 //   {
 //       .name            = "NamaTask",           // max 15 karakter
 //       .period_ms       = <periode>,             // > 0
 //       .phase_ms        = <offset awal>,         // 0 = tanpa phase
 //       .deadline_ms     = <deadline>,            // 0 = tidak ada deadline
 //       .priority        = RT_PRIO_*,             // BACKGROUND..REALTIME
 //       .core            = RT_CORE_*,             // 0, 1, atau ANY
 //       .stack_size      = <bytes>,               // minimal 2048
 //       .callback        = <nama_job>,            // void (*)(void *)
 //       .arg             = NULL,                  // biasanya NULL
 //       .enabled_on_boot = true,                  // aktif saat start
 //   },
 //
 // Urutan entry HARUS sama dengan enum task_index_t di task_registry.h.
 // =========================================================================
 static const rt_task_config_t s_task_table[] = {
     // ---- 0: TASK_IDX_COM_DISPATCH ----
     {
         .name            = "COM_Dispatch",
         .period_ms       = 1,
         .phase_ms        = 0,
         .deadline_ms     = 1,
         .priority        = RT_PRIO_REALTIME,
         .core            = RT_CORE_1,
         .stack_size      = 4096,
         .callback        = job_com_dispatch,
         .arg             = NULL,
         .enabled_on_boot = true,
     },

     // ---- 1: TASK_IDX_SYS_DIAG ----
     {
         .name            = "SYS_Diag",
         .period_ms       = 1000,
         .phase_ms        = 100,
         .deadline_ms     = 0,
         .priority        = RT_PRIO_BACKGROUND,
         .core            = RT_CORE_0,
         .stack_size      = 3072,
         .callback        = job_system_diagnostics,
         .arg             = NULL,
         .enabled_on_boot = true,
     },


 };

 _Static_assert(
     sizeof(s_task_table) / sizeof(s_task_table[0]) == TASK_IDX_COUNT,
     "task_registry: ukuran s_task_table tidak cocok dengan TASK_IDX_COUNT. "
     "Pastikan enum di task_registry.h dan tabel di task_registry.c sinkron."
 );

 static rt_task_id_t s_task_ids[TASK_IDX_COUNT];
 static bool s_initialized = false;

 esp_err_t task_registry_init_all(void) {
     if (s_initialized) {
         ESP_LOGW(TAG, "task_registry sudah pernah diinisialisasi.");
         return ESP_OK;
     }

     esp_err_t err = init_application();
     if (err != ESP_OK) {
         ESP_LOGE(TAG, "Gagal inisialisasi aplikasi: %s", esp_err_to_name(err));
         return err;
     }

     for (size_t i = 0; i < TASK_IDX_COUNT; i++) {
         s_task_ids[i] = RT_INVALID_TASK_ID;
     }

     for (size_t i = 0; i < TASK_IDX_COUNT; i++) {
         rt_task_id_t id = RT_INVALID_TASK_ID;
         err = rt_scheduler_register_task(&s_task_table[i], &id);

         if (err != ESP_OK) {
             ESP_LOGE(TAG, "Gagal register task[%zu] '%s': %s",
                      i, s_task_table[i].name, esp_err_to_name(err));
             return err;
         }

         s_task_ids[i] = id;
         ESP_LOGI(TAG, "Registered [%zu] '%s' (ID=%u)",
                  i, s_task_table[i].name, id);
     }

     s_initialized = true;
     ESP_LOGI(TAG, "Total %zu task berhasil didaftarkan.", TASK_IDX_COUNT);
     return ESP_OK;
 }

 rt_task_id_t task_registry_get_id(task_index_t idx) {
     if (idx >= TASK_IDX_COUNT) {
         return RT_INVALID_TASK_ID;
     }
     return s_task_ids[idx];
 }


