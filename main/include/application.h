/*
 * application.h
 *
 *  Created on: 18 Sept 2026
 *      Author: Rafdi
 */
 #ifndef MAIN_INCLUDE_APPLICATION_H_
 #define MAIN_INCLUDE_APPLICATION_H_

 #pragma once

 #include "Scheduler.h"
 #include "app_module.h"
 #include "main.h"
 #include "COM.h"
 #include "ringbuff_com.h"
 #include "IMU_MPU.h"
 #include "IoT_Response.h"
 #include "LCD.h"
 #include "com_can.h"
 #include "com_modbus_tcp.h"
 #include "com_wifi.h"
 #include "esp_log.h"
 #include "freertos/task.h"
 #include <stdint.h>
 #include <string.h>
 
 typedef enum {
   RAF_TASK_IDX_COM_DISPATCH = 0,
   RAF_TASK_IDX_SYS_DIAG,
   RAF_TASK_IDX_IMU_POLL,
   RAF_TASK_IDX_LCD,

   RAF_TASK_IDX_COUNT          // sentinel
 } RAF_TaskIndex_t;

 /**
  * @brief Inisialisasi infrastruktur dan seluruh modul aplikasi.
  */
 esp_err_t RAF_ApplicationInit(void);

 /**
  * @brief Ambil task ID runtime dari index enum.
  * @param idx Index dari enum task_index_t.
    * @return Task ID valid, atau RAF_RT_INVALID_TASK_ID jika idx salah.
  */
 RAF_TaskId_t RAF_TaskRegistryGetId(RAF_TaskIndex_t idx);



#endif /* MAIN_INCLUDE_APPLICATION_H_ */
