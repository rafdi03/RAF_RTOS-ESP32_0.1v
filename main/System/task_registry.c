/*
 * task_registry.c
 *
 *  Created on: 18 Sept 2026
 *      Author: Rafdi
 */


 #include "task_registry.h"
 #include "task.h"
 #include "esp_log.h"

 static const char *TAG = "TASK_REGISTRY";

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
         .callback        = job_1ms,
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
         .callback        = job_1000ms,
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

     for (size_t i = 0; i < TASK_IDX_COUNT; i++) {
         s_task_ids[i] = RT_INVALID_TASK_ID;
     }

     for (size_t i = 0; i < TASK_IDX_COUNT; i++) {
         rt_task_id_t id = RT_INVALID_TASK_ID;
         esp_err_t err = rt_scheduler_register_task(&s_task_table[i], &id);

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


