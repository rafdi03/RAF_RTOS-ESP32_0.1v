/*
 * Scheduler.h
 *
 *  Created on: 4 Sept 2026
 *      Author: Rafdi
 */

#ifndef MAIN_SCHEDULER_H_
#define MAIN_SCHEDULER_H_

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include <string.h>
#include <inttypes.h> 

#define RT_MAX_TASKS          16
#define RT_TASK_NAME_MAX_LEN  16

typedef uint16_t rt_task_id_t;
#define RT_INVALID_TASK_ID    0xFFFF

typedef enum {
    RT_STATE_UNINITIALIZED = 0,
    RT_STATE_INITIALIZED,
    RT_STATE_REGISTERING,
    RT_STATE_READY,
    RT_STATE_RUNNING,
    RT_STATE_STOPPED,
    RT_STATE_ERROR
} rt_scheduler_state_t;

typedef enum {
    RT_PRIO_BACKGROUND = 1, // Core 0 - Low Priority
    RT_PRIO_LOW        = 2, // Core 0
    RT_PRIO_MID        = 3, // Core 1 - General Task
    RT_PRIO_HIGH       = 4, // Core 1 - Fast Sensor / Control
    RT_PRIO_REALTIME   = 5  // Core 1 - Time Critical
} rt_priority_t;

typedef enum {
    RT_CORE_0   = 0,
    RT_CORE_1   = 1,
    RT_CORE_ANY = -1
} rt_core_t;

typedef void (*rt_task_callback_t)(void *arg);

// Task Descriptor Configuration
typedef struct {
    char name[RT_TASK_NAME_MAX_LEN];

    uint32_t period_ms;         // Periode eksekusi (ms)
    uint32_t phase_ms;          // Offset eksekusi awal (ms) untuk mencegah CPU burst
    uint32_t deadline_ms;       // Batas toleransi durasi eksekusi (ms)

    rt_priority_t priority;     // FreeRTOS Priority (1-5)
    rt_core_t core;             // Core Affinity (0, 1, atau ANY)
    uint32_t stack_size;        // Ukuran stack (Bytes)

    rt_task_callback_t callback;// Function pointer logika aplikasi
    void *arg;                  // Parameter callback
    bool enabled_on_boot;       // Status aktif saat scheduler start
} rt_task_config_t;

// Real-Time Metrics & Profiler
typedef struct {
    rt_task_id_t id;
    char name[RT_TASK_NAME_MAX_LEN];
    bool enabled;

    uint32_t execution_count;
    uint32_t execution_overruns; // T_exec > deadline_ms
    uint32_t deadline_misses;    // Terlambat dipanggil relatif terhadap periode

    uint32_t min_exec_us;
    uint32_t max_exec_us;
    uint32_t avg_exec_us;
    uint32_t last_exec_us;
    uint32_t max_jitter_us;
} rt_task_stats_t;

// API Lifecycle Engine
esp_err_t init_scheduler_engine(void);
esp_err_t rt_scheduler_register_task(const rt_task_config_t *config, rt_task_id_t *out_task_id);
esp_err_t rt_scheduler_start(void);
esp_err_t rt_scheduler_stop(void);
rt_scheduler_state_t rt_scheduler_get_state(void);

// Runtime Control & Profiler API
esp_err_t rt_task_enable(rt_task_id_t task_id);
esp_err_t rt_task_disable(rt_task_id_t task_id);
esp_err_t rt_task_get_stats(rt_task_id_t task_id, rt_task_stats_t *out_stats);
void rt_scheduler_print_stats(void);

#endif /* MAIN_SCHEDULER_H_ */
