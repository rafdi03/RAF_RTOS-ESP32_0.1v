/*
 * main.h
 *
 *  Created on: 7 Sept 2026
 *      Author: Rafdi
 */

#ifndef MAIN_INCLUDE_MAIN_H_
#define MAIN_INCLUDE_MAIN_H_

#include <stdint.h>
#include "Scheduler.h"
#include "COM.h"
#include "ringbuff_com.h"
#include "esp_log.h"
#include "task.h"
#include "task_registry.h"

#pragma once

#define WIFI_SSID_DEFAULT           "Rumah Kita"
#define WIFI_PASS_DEFAULT           "EKAGUNAPUTRA03"
#define MQTT_BROKER_URI_DEFAULT     "mqtt://broker.emqx.io:1883" 
#define MQTT_CLIENT_ID_DEFAULT      "ESP32_NODE1_01"
#define MQTT_TOPIC_REQ_DEFAULT      "esp32/node1/request"
#define MQTT_TOPIC_RESP_DEFAULT     "esp32/node1/response"


#define I2C_SPEED_STANDARD          100000   
#define I2C_SPEED_FAST              400000   
#define I2C_SPEED_FAST_PLUS         1000000 

// Preset Kecepatan Tertinggi: 400 kHz Fast Mode
#define I2C_SPEED_IMU_DEFAULT       I2C_SPEED_FAST
#define I2C_SPEED_LCD_DEFAULT       I2C_SPEED_FAST

#define CAN_BAUD_RATE_MAX           1000     // 1000 kbps (1 Mbps - Maksimum TWAI Controller)
#define UART_BAUD_RATE_MAX          921600   // 921.600 bps (Maksimum UART)


typedef struct {
    int sda;
    int scl;
} bsp_i2c_pins_t;
	
typedef struct {
    int mosi;
    int miso;
    int sck;
    int cs;
} bsp_spi_pins_t;

typedef struct {
    int tx;
    int rx;
} bsp_uart_pins_t;

typedef struct {
    int tx;
    int rx;
} bsp_can_pins_t;

#define I2C_PINS_LCD   ((bsp_i2c_pins_t){ .sda = 26, .scl = 25 })
#define I2C_PINS_IMU   ((bsp_i2c_pins_t){ .sda = 14, .scl = 12 })

#define UART_PINS_GPS  ((bsp_uart_pins_t){ .tx = 17, .rx = 16 })
#define SPI_PINS_LORA  ((bsp_spi_pins_t){ .mosi = 23, .miso = 19, .sck = 18, .cs = 5 })
#define CAN_PINS_DEFAULT ((bsp_can_pins_t){ .tx = 21, .rx = 22 })



#endif /* MAIN_INCLUDE_MAIN_H_ */
