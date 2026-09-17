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
//	data_logger_init();

    com_wifi_init(WIFI_SSID_DEFAULT, WIFI_PASS_DEFAULT);
}

void job_1ms(void) {
    com_update_1ms(); 
	com_can_rx_poll();
}

void job_5ms(void) {

}

void job_10ms(void) {
    //imu_mpu_update();
}

void job_15ms(void) {

}

void job_20ms(void) {
	
}


void job_50ms(void) {

}

void job_100ms(void) {
    
}

void job_200ms(void) {
	
}

void job_300ms(void) {
 
}

void job_500ms(void) {
  
}

void job_1000ms(void) {
    ringbuf_com_print_stats();
	static uint16_t tick = 0;
	    if (++tick >= 10) {         
	        tick = 0;
	        send_mqtt_json();       
	    }
}


#if 0 
static void IRAM_ATTR isr_level_1_3_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    
    // Contoh mengirim event dari ISR ke RingBuffer secara aman:
    // BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // ringbuf_com_send_from_isr(&gpio_num, sizeof(gpio_num), &xHigherPriorityTaskWoken);
    // if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

void init_interrupt_level_1_3(gpio_num_t gpio_pin, gpio_int_type_t intr_type) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = intr_type
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
    gpio_isr_handler_add(gpio_pin, isr_level_1_3_handler, (void*) gpio_pin);
}
#endif

#if 0
static void IRAM_ATTR isr_level_4_5_handler(void* arg) {
    // Logika Hard Real-Time
}

void init_interrupt_level_4_5(gpio_num_t gpio_pin) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&io_conf);
    esp_intr_alloc(ETS_GPIO_INTR_SOURCE, ESP_INTR_FLAG_LEVEL5 | ESP_INTR_FLAG_IRAM, 
                   isr_level_4_5_handler, NULL, NULL);
}
#endif

void input_init(){
	imu_mpu_init(NULL);
}
