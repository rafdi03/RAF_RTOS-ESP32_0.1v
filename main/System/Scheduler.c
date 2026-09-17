/*
 * Scheduler.c
 *
 *  Created on: 4 Sept 2026
 *      Author: Rafdi
 */

#include "Scheduler.h"


static const char *TAG = "SCHEDULER";

typedef struct {
    rt_task_id_t id;
    rt_task_config_t config;
    TaskHandle_t free_rtos_handle;

    // Internal counters & Profiling
    volatile bool enabled;
    uint32_t execution_count;
    uint32_t execution_overruns;
    uint32_t deadline_misses;
	
	uint32_t missed_periods;

    uint32_t min_exec_us;
    uint32_t max_exec_us;
    uint64_t total_exec_us;
    uint32_t last_exec_us;
    uint32_t max_jitter_us;

    SemaphoreHandle_t lock; // Thread-safe snapshot isolation
    bool is_used;
} rt_task_slot_t;

static rt_task_slot_t s_task_slots[RT_MAX_TASKS];
static size_t s_task_count = 0;
static rt_scheduler_state_t s_scheduler_state = RT_STATE_UNINITIALIZED;
static SemaphoreHandle_t s_registry_lock = NULL;

static esp_err_t init_task_watchdog(uint32_t timeout_ms) {
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = timeout_ms,
        .idle_core_mask = (1 << 0) | (1 << 1),
        .trigger_panic = true
    };

    esp_err_t err = esp_task_wdt_reconfigure(&twdt_config);
    if (err != ESP_OK) {
        err = esp_task_wdt_init(&twdt_config);
    }
    return err;
	
	ESP_LOGI(TAG, "WDT dikonfigurasi: timeout %lu ms", (unsigned long)timeout_ms);
}

static esp_err_t validate_task_config(const rt_task_config_t *config) {
    if (config == NULL || config->callback == NULL) {
        ESP_LOGE(TAG, "Config Invalid: config atau callback NULL");
        return ESP_ERR_INVALID_ARG;
    }
	if (config->name[0] == '\0') {
	    ESP_LOGE(TAG, "Config Invalid: name kosong");
	    return ESP_ERR_INVALID_ARG;
	}
    if (config->period_ms == 0) {
        ESP_LOGE(TAG, "Config Invalid: period_ms tidak boleh 0");
        return ESP_ERR_INVALID_ARG;
    }

    TickType_t period_ticks = pdMS_TO_TICKS(config->period_ms);
    if (period_ticks == 0) {
        ESP_LOGE(TAG, "Config Invalid: period_ms %lu terlalu kecil untuk tick HZ %d",
                 (unsigned long)config->period_ms, configTICK_RATE_HZ);
        return ESP_ERR_INVALID_ARG;
    }

    // Peringatan jika period tidak presisi di tick rate saat ini
    uint32_t actual_ms = (uint32_t)((period_ticks * 1000U) / configTICK_RATE_HZ);
    if (actual_ms != config->period_ms) {
        ESP_LOGW(TAG, "period_ms %lu tidak presisi, aktual %lu ms (tick %d Hz)",
                 (unsigned long)config->period_ms,
                 (unsigned long)actual_ms,
                 configTICK_RATE_HZ);
    }

    if (config->deadline_ms > 0 && config->deadline_ms > config->period_ms) {
        ESP_LOGE(TAG, "Config Invalid: deadline_ms (%lu) > period_ms (%lu)",
                 (unsigned long)config->deadline_ms,
                 (unsigned long)config->period_ms);
        return ESP_ERR_INVALID_ARG;
    }
    if (config->priority < RT_PRIO_BACKGROUND || config->priority > RT_PRIO_REALTIME) {
        ESP_LOGE(TAG, "Config Invalid: priority %d di luar range", config->priority);
        return ESP_ERR_INVALID_ARG;
    }
    if (config->stack_size > 0 && config->stack_size < 2048) {
        ESP_LOGE(TAG, "Config Invalid: stack_size %lu terlalu kecil",
                 (unsigned long)config->stack_size);
        return ESP_ERR_INVALID_ARG;
    }
    if (config->core != RT_CORE_ANY &&
        config->core != RT_CORE_0 &&
        config->core != RT_CORE_1) {
        ESP_LOGE(TAG, "Config Invalid: core %d tidak valid", config->core);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static void rt_generic_task_wrapper(void *pvParameters) {
    rt_task_slot_t *slot = (rt_task_slot_t *)pvParameters;

    if (slot->config.phase_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(slot->config.phase_ms));
    }

    esp_task_wdt_add(NULL);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(slot->config.period_ms);

    while (1) {
        bool en;
        xSemaphoreTake(slot->lock, portMAX_DELAY);
        en = slot->enabled;
        xSemaphoreGive(slot->lock);

        if (en) {
            int64_t expected_us =
                (int64_t)xLastWakeTime * 1000000LL / configTICK_RATE_HZ;
            int64_t start_time_us = esp_timer_get_time();

            slot->config.callback(slot->config.arg);

            int64_t end_time_us = esp_timer_get_time();
            uint32_t exec_duration_us = (uint32_t)(end_time_us - start_time_us);

            xSemaphoreTake(slot->lock, portMAX_DELAY);

            slot->execution_count++;
            slot->last_exec_us = exec_duration_us;
            slot->total_exec_us += exec_duration_us;

            if (exec_duration_us < slot->min_exec_us) slot->min_exec_us = exec_duration_us;
            if (exec_duration_us > slot->max_exec_us) slot->max_exec_us = exec_duration_us;

            if (slot->config.deadline_ms > 0 &&
                exec_duration_us > (uint64_t)slot->config.deadline_ms * 1000ULL) {
                slot->execution_overruns++;
            }

            int64_t jitter_us = start_time_us - expected_us;
            if (jitter_us < 0) jitter_us = -jitter_us;
            if (jitter_us > (int64_t)slot->max_jitter_us) {
                slot->max_jitter_us = (uint32_t)jitter_us;
            }

            if (slot->config.deadline_ms > 0) {
                int64_t deadline_us =
                    expected_us + (int64_t)slot->config.deadline_ms * 1000LL;
                if (end_time_us > deadline_us) {
                    slot->deadline_misses++;
                }
            }

            xSemaphoreGive(slot->lock);
        }

        esp_task_wdt_reset();

        TickType_t tick_before = xLastWakeTime;
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        TickType_t now = xTaskGetTickCount();
        if ((now - tick_before) > xFrequency) {
            xSemaphoreTake(slot->lock, portMAX_DELAY);
            slot->missed_periods++;
            xSemaphoreGive(slot->lock);
        }
    }
}

esp_err_t init_scheduler_engine(void) {
    if (s_scheduler_state != RT_STATE_UNINITIALIZED) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(s_task_slots, 0, sizeof(s_task_slots));
    s_task_count = 0;

    s_registry_lock = xSemaphoreCreateMutex();
    if (s_registry_lock == NULL) {
        s_scheduler_state = RT_STATE_ERROR;
        ESP_LOGE(TAG, "Gagal membuat registry mutex");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = init_task_watchdog(3000);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_registry_lock);
        s_registry_lock = NULL;
        s_scheduler_state = RT_STATE_ERROR;
        ESP_LOGE(TAG, "Gagal init WDT: %s", esp_err_to_name(err));
        return err;
    }

    s_scheduler_state = RT_STATE_INITIALIZED;
    ESP_LOGI(TAG, "RTOS Scheduler Engine initialized.");
    return ESP_OK;
}

esp_err_t rt_scheduler_register_task(const rt_task_config_t *config, rt_task_id_t *out_task_id) {
    if (s_scheduler_state != RT_STATE_INITIALIZED &&
        s_scheduler_state != RT_STATE_REGISTERING) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = validate_task_config(config);
    if (err != ESP_OK) return err;

    xSemaphoreTake(s_registry_lock, portMAX_DELAY);

    if (s_task_count >= RT_MAX_TASKS) {
        xSemaphoreGive(s_registry_lock);
        return ESP_ERR_NO_MEM;
    }

    rt_task_slot_t *slot = &s_task_slots[s_task_count];
    memset(slot, 0, sizeof(rt_task_slot_t));

    memcpy(&slot->config, config, sizeof(rt_task_config_t));

    slot->config.name[RT_TASK_NAME_MAX_LEN - 1] = '\0';

    slot->id = (rt_task_id_t)(s_task_count + 1);
    slot->enabled = config->enabled_on_boot;
    slot->min_exec_us = UINT32_MAX;

    slot->lock = xSemaphoreCreateMutex();
    if (slot->lock == NULL) {
        memset(slot, 0, sizeof(rt_task_slot_t));
        xSemaphoreGive(s_registry_lock);
        return ESP_ERR_NO_MEM;
    }

    slot->is_used = true;
    if (out_task_id) *out_task_id = slot->id;

    s_task_count++;
    s_scheduler_state = RT_STATE_REGISTERING;

    xSemaphoreGive(s_registry_lock);

    ESP_LOGI(TAG, "Task Registered -> ID: %u | Name: %s | Period: %lu ms | Phase: %lu ms",
             slot->id, slot->config.name,
             (unsigned long)slot->config.period_ms,
             (unsigned long)slot->config.phase_ms);

    return ESP_OK;
}

esp_err_t rt_scheduler_start(void) {
    if (s_scheduler_state != RT_STATE_REGISTERING &&
        s_scheduler_state != RT_STATE_INITIALIZED) {
        return ESP_ERR_INVALID_STATE;
    }

    // Hitung periode maksimum untuk WDT
    uint32_t max_period = 0;
    for (size_t i = 0; i < s_task_count; i++) {
        if (s_task_slots[i].config.period_ms > max_period) {
            max_period = s_task_slots[i].config.period_ms;
        }
    }
	uint32_t wdt_timeout = max_period + 1000;
	if (wdt_timeout < 3000) wdt_timeout = 3000;

    xSemaphoreTake(s_registry_lock, portMAX_DELAY);

    size_t created = 0;
    for (size_t i = 0; i < s_task_count; i++) {
        rt_task_slot_t *slot = &s_task_slots[i];
        uint32_t stack = (slot->config.stack_size > 0) ? slot->config.stack_size : 3072;
        BaseType_t core_affinity = (slot->config.core == RT_CORE_ANY)
                                   ? tskNO_AFFINITY
                                   : (BaseType_t)slot->config.core;

        BaseType_t res = xTaskCreatePinnedToCore(
            rt_generic_task_wrapper,
            slot->config.name,
            stack,
            (void *)slot,
            slot->config.priority,
            &slot->free_rtos_handle,
            core_affinity);

        if (res != pdPASS) {
            ESP_LOGE(TAG, "Gagal create task: %s", slot->config.name);
            // Rollback
            for (size_t j = 0; j < created; j++) {
                if (s_task_slots[j].free_rtos_handle) {
                    vTaskDelete(s_task_slots[j].free_rtos_handle);
                    s_task_slots[j].free_rtos_handle = NULL;
                }
            }
            s_scheduler_state = RT_STATE_ERROR;
            xSemaphoreGive(s_registry_lock);
            return ESP_FAIL;
        }
        created++;
    }

    s_scheduler_state = RT_STATE_RUNNING;
    xSemaphoreGive(s_registry_lock);

    ESP_LOGI(TAG, "Scheduler RUNNING dengan %zu task aktif.", s_task_count);
    return ESP_OK;
}

rt_scheduler_state_t rt_scheduler_get_state(void) {
    return s_scheduler_state;
}

esp_err_t rt_task_enable(rt_task_id_t task_id) {
    if (task_id == 0 || task_id > s_task_count) return ESP_ERR_INVALID_ARG;
    rt_task_slot_t *slot = &s_task_slots[task_id - 1];
    if (slot->lock == NULL) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(slot->lock, portMAX_DELAY);
    slot->enabled = true;
    xSemaphoreGive(slot->lock);
    return ESP_OK;
}

esp_err_t rt_task_disable(rt_task_id_t task_id) {
    if (task_id == 0 || task_id > s_task_count) return ESP_ERR_INVALID_ARG;
    rt_task_slot_t *slot = &s_task_slots[task_id - 1];
    if (slot->lock == NULL) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(slot->lock, portMAX_DELAY);
    slot->enabled = false;
    xSemaphoreGive(slot->lock);
    return ESP_OK;
}

esp_err_t rt_task_get_stats(rt_task_id_t task_id, rt_task_stats_t *out_stats) {
    if (task_id == 0 || task_id > s_task_count || out_stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    rt_task_slot_t *slot = &s_task_slots[task_id - 1];
    if (slot->lock == NULL) return ESP_ERR_INVALID_STATE;

    memset(out_stats, 0, sizeof(*out_stats));

    xSemaphoreTake(slot->lock, portMAX_DELAY);

    out_stats->id = slot->id;
    strncpy(out_stats->name, slot->config.name, RT_TASK_NAME_MAX_LEN - 1);
    out_stats->name[RT_TASK_NAME_MAX_LEN - 1] = '\0';
    out_stats->enabled = slot->enabled;
    out_stats->execution_count = slot->execution_count;
    out_stats->execution_overruns = slot->execution_overruns;
    out_stats->deadline_misses = slot->deadline_misses;
    out_stats->missed_periods = slot->missed_periods;
    out_stats->min_exec_us = (slot->min_exec_us == UINT32_MAX) ? 0 : slot->min_exec_us;
    out_stats->max_exec_us = slot->max_exec_us;
    out_stats->last_exec_us = slot->last_exec_us;
    out_stats->avg_exec_us = (slot->execution_count > 0)
        ? (uint32_t)(slot->total_exec_us / slot->execution_count) : 0;
    out_stats->max_jitter_us = slot->max_jitter_us;

    xSemaphoreGive(slot->lock);
    return ESP_OK;
}

void rt_scheduler_print_stats(void) {
    ESP_LOGI(TAG, "=================================== INDUSTRIAL RTOS METRICS SNAPSHOT ===================================");
    ESP_LOGI(TAG, "%-3s | %-10s | %-3s | %-8s | %-8s | %-8s | %-8s | %-8s | %-8s | %-8s",
             "ID", "Name", "En", "Min(us)", "Avg(us)", "Max(us)",
             "Jitter", "Overrun", "Deadline", "Missed");
    ESP_LOGI(TAG, "--------------------------------------------------------------------------------------------------------");

    for (size_t i = 0; i < s_task_count; i++) {
        rt_task_stats_t st;
        if (rt_task_get_stats((rt_task_id_t)(i + 1), &st) == ESP_OK) {
            ESP_LOGI(TAG, "%-3" PRIu32 " | %-10s | %-3s | %-8" PRIu32
                          " | %-8" PRIu32 " | %-8" PRIu32
                          " | %-8" PRIu32 " | %-8" PRIu32 " | %-8" PRIu32
                          " | %-8" PRIu32,
			         st.id, st.name, st.enabled ? "Y" : "N",
			         st.min_exec_us, st.avg_exec_us, st.max_exec_us,
                     st.max_jitter_us, st.execution_overruns,
                     st.deadline_misses, st.missed_periods);
        }
    }
    ESP_LOGI(TAG, "================================================================================------------------------");
}
