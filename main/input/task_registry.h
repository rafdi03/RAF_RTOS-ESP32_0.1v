/*
 * task_registry.h
 *
 *  Created on: 18 Sept 2026
 *      Author: Rafdi
 */
 #ifndef MAIN_INCLUDE_TASK_REGISTRY_H_
 #define MAIN_INCLUDE_TASK_REGISTRY_H_

 #pragma once

 #include "Scheduler.h"

 typedef enum {
     TASK_IDX_COM_DISPATCH = 0,
     TASK_IDX_SYS_DIAG,

     // TASK_IDX_INA_POLL,
     // TASK_IDX_GPS_POLL,

     TASK_IDX_COUNT          // sentinel
 } task_index_t;

 /**
  * @brief Register semua task dari tabel ke scheduler engine.
  * @note Panggil setelah init_scheduler_engine() dan sebelum rt_scheduler_start().
  * @return ESP_OK jika semua task berhasil didaftarkan.
  */
 esp_err_t task_registry_init_all(void);

 /**
  * @brief Ambil task ID runtime dari index enum.
  * @param idx Index dari enum task_index_t.
  * @return Task ID valid, atau RT_INVALID_TASK_ID jika idx salah.
  */
 rt_task_id_t task_registry_get_id(task_index_t idx);



#endif /* MAIN_INPUT_TASK_REGISTRY_H_ */
