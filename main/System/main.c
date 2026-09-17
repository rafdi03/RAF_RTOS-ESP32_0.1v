/*
 * main.c
 *
 *  Created on: 4 Sept 2026
 *      Author: Rafdi
 */
#include "main.h"
#include "task_registry.h"
#include "esp_err.h"

 
 void app_main(void) {
     ESP_ERROR_CHECK(init_scheduler_engine());
     ESP_ERROR_CHECK(task_registry_init_all());
     ESP_ERROR_CHECK(rt_scheduler_start());

 }