/*
 * task.c
 *
 *  Created on: 4 Sept 2026
 *      Author: Rafdi
 */

 #include "task.h"

 static const char *TAG __attribute__((unused)) = "TASK_JOBS";
 static volatile int_callback_t in_user_cb = NULL;

 void register_int_callback(int_callback_t cb) {
     in_user_cb = cb;
 }

 void execute_int_callback(void) {
     int_callback_t cb = (int_callback_t)in_user_cb;
     if (cb != NULL) {
         cb();
     }
 }
 
 void startup_application(void) {
     com_init();
     input_init();
     iot_response_init();
 //  data_logger_init();

     com_wifi_init(WIFI_SSID_DEFAULT, WIFI_PASS_DEFAULT);
 }

 void input_init(void) {
     imu_mpu_init(NULL);
 }


 /**
  * @brief Job 1 ms — Communication Dispatcher (High Priority, Core 1)
  * @note WAJIB ringan. Idealnya < 200 us.
  */
 void job_1ms(void *arg) {
     (void)arg;
     com_update_1ms();
     com_can_rx_poll();
 }

 void job_5ms(void *arg) {
     (void)arg;
 }

 void job_10ms(void *arg) {
     (void)arg;
 }

 void job_15ms(void *arg) {
     (void)arg;
 }

 void job_20ms(void *arg) {
     (void)arg;
 }

 void job_50ms(void *arg) {
     (void)arg;
 }

 void job_100ms(void *arg) {
     (void)arg;
 }

 void job_200ms(void *arg) {
     (void)arg;
 }

 void job_300ms(void *arg) {
     (void)arg;
 }

 void job_500ms(void *arg) {
     (void)arg;
 }

 /**
  * @brief Job 1000 ms — System Diagnostics & Cloud Telemetry
  */
 void job_1000ms(void *arg) {
     (void)arg;

     ringbuf_com_print_stats();
     rt_scheduler_print_stats();

     static uint16_t tick = 0;
     if (++tick >= 10) {
         tick = 0;
         send_mqtt_json();
     }
 }