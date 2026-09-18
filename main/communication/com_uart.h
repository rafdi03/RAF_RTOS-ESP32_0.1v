/*
 * com_uart.h
 *
 *  Created on: 14 Sept 2026
 *      Author: Rafdi
 *
 *  Deskripsi: Modul komunikasi UART / RS485 Industrial.
 */

#ifndef MAIN_COMMUNICATION_COM_UART_H_
#define MAIN_COMMUNICATION_COM_UART_H_

#pragma once
#include "esp_err.h"
#include "bsp_pins.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inisialisasi peripheral UART/RS485 dengan konfigurasi baud rate dan pin BSP.
 *        Otomatis meregistrasikan TX handler ke Central Com Hub.
 * @param pins Konfigurasi pin TX & RX.
 * @param baud_rate Kecepatan komunikasi (contoh: 115200, 9600 bps).
 * @return ESP_OK jika berhasil.
 */
esp_err_t com_uart_init(bsp_uart_pins_t pins, uint32_t baud_rate);

/**
 * @brief Mengirimkan byte data balasan melalui antarmuka UART/RS485 (TX Handler).
 * @param data Pointer ke buffer data yang akan dikirimkan.
 * @param len Jumlah byte data yang akan ditransmisikan.
 * @return ESP_OK jika berhasil.
 */
esp_err_t com_uart_send(const void *data, size_t len);

/**
 * @brief Callback event penampung data byte masuk dari UART ISR/Driver ke Com Hub.
 * @param bytes Pointer ke data byte yang diterima dari UART hardware.
 * @param len Panjang data byte yang diterima.
 */
void com_uart_on_rx_bytes(const uint8_t *bytes, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_COMMUNICATION_COM_UART_H_ */
