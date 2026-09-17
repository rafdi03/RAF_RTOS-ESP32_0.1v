/*
 * LCD.c
 *
 *  Created on: 4 Sept 2026
 *      Author: Rafdi
 */

#include "LCD.h"


#define LCD_BACKLIGHT    0x08
#define LCD_ENABLE_BIT   0x04

// Private encapsulated handles (OOP Information Hiding)
static i2c_master_dev_handle_t s_lcd_dev_handle = NULL;
static i2c_master_bus_handle_t s_lcd_bus_handle = NULL;

static esp_err_t pcf8574_write(uint8_t data) {
    if (s_lcd_dev_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit(s_lcd_dev_handle, &data, 1, pdMS_TO_TICKS(50));
}

static void lcd_write_nibble(uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;
    pcf8574_write(data | LCD_ENABLE_BIT);
    esp_rom_delay_us(50);
    pcf8574_write(data & ~LCD_ENABLE_BIT);
    esp_rom_delay_us(50);
}

void lcd_send_cmd(uint8_t cmd) {
    lcd_write_nibble(cmd & 0xF0, 0x00);        // 4-bit Atas (Mode command = 0)
    lcd_write_nibble((cmd << 4) & 0xF0, 0x00); // 4-bit Bawah
}

void lcd_send_data(uint8_t data) {
    lcd_write_nibble(data & 0xF0, 0x01);        // 4-bit Atas (Mode data = 1)
    lcd_write_nibble((data << 4) & 0xF0, 0x01); // 4-bit Bawah
}

void lcd_clear(void) {
    lcd_send_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd_put_cur(int row, int col) {
    switch (row) {
        case 0: col |= 0x80; break;
        case 1: col |= 0xC0; break;
    }
    lcd_send_cmd(col);
}

void lcd_send_string(const char *str) {
    if (str == NULL) return;
    while (*str) {
        lcd_send_data((uint8_t)(*str++));
    }
}

esp_err_t lcd_init(const lcd_config_t *config) {
    lcd_config_t default_cfg;
    if (config == NULL) {
        default_cfg = LCD_CONFIG_DEFAULT();
        config = &default_cfg;
    }
	
    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1, 
        .sda_io_num = config->pins.sda,
        .scl_io_num = config->pins.scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&bus_config, &s_lcd_bus_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    // 2. Tambahkan device LCD ke bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_addr,
        .scl_speed_hz = I2C_SPEED_LCD_DEFAULT, // 400kHz Fast Mode
    };
    ret = i2c_master_bus_add_device(s_lcd_bus_handle, &dev_cfg, &s_lcd_dev_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_write_nibble(0x30, 0x00);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x30, 0x00);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write_nibble(0x30, 0x00);
    vTaskDelay(pdMS_TO_TICKS(10));
    lcd_write_nibble(0x20, 0x00); // Set ke 4-bit mode
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_send_cmd(0x28); // 2 baris, font 5x8
    lcd_send_cmd(0x08); // Display OFF
    lcd_clear();
    lcd_send_cmd(0x06); // Auto-increment kursor
    lcd_send_cmd(0x0C); // Display ON, kursor OFF

    return ESP_OK;
}

esp_err_t lcd_init_pins(bsp_i2c_pins_t pins) {
    lcd_config_t cfg = {
        .pins = pins,
        .i2c_addr = LCD_DEFAULT_ADDR,
    };
    return lcd_init(&cfg);
}
