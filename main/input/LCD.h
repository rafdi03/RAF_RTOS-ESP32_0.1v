/*
 * LCD.h
 *
 *  Created on: 4 Sept 2026
 *      Author: Rafdi
 */

#ifndef MAIN_INCLUDE_LCD_H_
#define MAIN_INCLUDE_LCD_H_

#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "main.h"
#include <string.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"

#define LCD_DEFAULT_ADDR    0x27  // Ganti ke 0x3F jika LCD Anda menggunakan alamat 0x3F

// Struct konfigurasi modular untuk LCD
typedef struct {
    bsp_i2c_pins_t pins;
    uint8_t i2c_addr;
} lcd_config_t;

// Macro helper inisialisasi default menggunakan pin dari main.h
#define LCD_CONFIG_DEFAULT()  ((lcd_config_t){ .pins = I2C_PINS_LCD, .i2c_addr = LCD_DEFAULT_ADDR })

// Public API
esp_err_t lcd_init(const lcd_config_t *config);
esp_err_t lcd_init_pins(bsp_i2c_pins_t pins);
void lcd_send_cmd(uint8_t cmd);
void lcd_send_data(uint8_t data);	
void lcd_put_cur(int row, int col);
void lcd_send_string(const char *str);
void lcd_clear(void);

#endif /* MAIN_INCLUDE_LCD_H_ */
