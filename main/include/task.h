/*

* task.h
*
* Created on: 4 Sept 2026
* ```
   Author: Rafdi
  ```

*/
#ifndef MAIN_INCLUDE_TASK_H_
#define MAIN_INCLUDE_TASK_H_

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_log.h"
#include "main.h"
#include "LCD.h"
#include "IMU_MPU.h"
#include "ringbuff_com.h"
#include "COM.h"
#include "com_wifi.h"
#include "com_mqtt.h"
#include "com_ota.h"
#include "com_modbus_tcp.h"
#include "com_espnow.h"
#include "com_can.h"
#include "com_lora.h"
#include "com_uart.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include "data_logger.h"
#include "IoT_Response.h"

typedef void (*int_callback_t)(void);

void register_int_callback(int_callback_t cb);
void execute_int_callback(void);

void startup_application(void);
void input_init(void);


void job_1ms(void *arg);
void job_5ms(void *arg);
void job_10ms(void *arg);
void job_15ms(void *arg);
void job_20ms(void *arg);
void job_50ms(void *arg);
void job_100ms(void *arg);
void job_200ms(void *arg);
void job_300ms(void *arg);
void job_500ms(void *arg);
void job_1000ms(void *arg);

#endif /* MAIN_INCLUDE_TASK_H_ */