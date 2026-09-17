/*
 * main.c
 *
 *  Created on: 4 Sept 2026
 *      Author: Rafdi
 */
 #include "main.h"

 
 void app_main(void) {
     ESP_ERROR_CHECK(ringbuf_com_init(RINGBUF_COMM_DEFAULT_SIZE));

     startup_application();

	 ESP_ERROR_CHECK(init_scheduler_engine());
     ESP_ERROR_CHECK(task_registry_init_all());
     ESP_ERROR_CHECK(rt_scheduler_start());

 }