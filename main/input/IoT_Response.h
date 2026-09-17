/*
 * IoT_Response.h
 *
 *  Created on: 9 Sept 2026
 *      Author: Rafdi
 */

#ifndef MAIN_INPUT_IOT_RESPONSE_H_
#define MAIN_INPUT_IOT_RESPONSE_H_

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "Com.h"
#include "IMU_MPU.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inisialisasi dan registrasi IoT Response Handlers (JSON & Biner) ke Central Com Hub.
 *        Panggil fungsi ini sekali saat startup aplikasi (misal di startup_application()).
 */
void iot_response_init(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_INPUT_IOT_RESPONSE_H_ */

