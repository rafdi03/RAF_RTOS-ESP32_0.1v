/*
 * application.c
 *
 *  Created on: 18 Sept 2026
 *      Author: Rafdi
 */

#include "application.h"

static const char *TAG = "RAF_APPLICATION";

// ============================================================================
// Module Init Wrappers
// ============================================================================
static esp_err_t RAF_ApplicationInitImu(void) {
    return imu_mpu_init(NULL);
}

static esp_err_t RAF_ApplicationInitLcd(void) {
    return lcd_init(NULL);
}

// ============================================================================
// Context Pattern — Konsisten untuk semua task
// ============================================================================
typedef void (*RAF_ModuleUpdate_t)(void *context);

typedef struct {
    RAF_ModuleUpdate_t update;
    void *context;
} RAF_ModuleCallbackContext_t;

static void RAF_ApplicationJobModule(void *arg) {
    RAF_ModuleCallbackContext_t *module = (RAF_ModuleCallbackContext_t *)arg;

    if (module == NULL || module->update == NULL) {
        return;
    }

    module->update(module->context);
}

// ============================================================================
// Update Functions per Modul
// ============================================================================
static void update_com_dispatch(void *context) {
    (void)context;
    com_update_1ms();
    com_can_rx_poll();
}

static void update_diagnostics(void *context) {
    (void)context;
    ringbuf_com_print_stats();
    RAF_SchedulerPrintStats();

    static uint16_t stats_tick = 0;
    if (++stats_tick >= 5) {
        stats_tick = 0;
        static char cpu_buf[1024];
        vTaskGetRunTimeStats(cpu_buf);
        ESP_LOGI("CPU", "\n%s", cpu_buf);
    }

    static uint16_t mqtt_tick = 0;
    if (++mqtt_tick >= 10) {
        mqtt_tick = 0;
        send_mqtt_json();
    }
}

static void update_imu(void *context) {
    (void)context;
    imu_mpu_update();
}

// ============================================================================
// Context Instances
// ============================================================================
static RAF_ModuleCallbackContext_t s_com_dispatch_context = {
    .update = update_com_dispatch,
    .context = NULL,
};

static RAF_ModuleCallbackContext_t s_diagnostics_context = {
    .update = update_diagnostics,
    .context = NULL,
};

static RAF_ModuleCallbackContext_t s_imu_context = {
    .update = update_imu,
    .context = NULL,
};

static const RAF_AppModule_t s_modules[] = {
    {
        .name = "COM_Dispatch",
        .task = {
            .name            = "COM_Dispatch",
            .period_ms       = 1,
            .phase_ms        = 0,
            .deadline_ms     = 1,
            .priority        = RAF_RT_PRIO_REALTIME,
            .core            = RAF_RT_CORE_1,
            .stack_size      = 4096,
            .callback        = RAF_ApplicationJobModule,
            .arg             = &s_com_dispatch_context,
            .enabled_on_boot = true,
        },
        .required = true,
    },
    {
        .name = "SYS_Diag",
        .task = {
            .name            = "SYS_Diag",
            .period_ms       = 1000,
            .phase_ms        = 100,
            .deadline_ms     = 0,
            .priority        = RAF_RT_PRIO_BACKGROUND,
            .core            = RAF_RT_CORE_0,
            .stack_size      = 3072,
            .callback        = RAF_ApplicationJobModule,
            .arg             = &s_diagnostics_context,  
            .enabled_on_boot = true,
        },
        .required = true,
    },
    {
        .name = "IMU_MPU6050",
        .init = RAF_ApplicationInitImu,
        .task = {
            .name            = "IMU_Poll",
            .period_ms       = 10,
            .phase_ms        = 0,
            .deadline_ms     = 0,
            .priority        = RAF_RT_PRIO_HIGH,
            .core            = RAF_RT_CORE_1,
            .stack_size      = 4096,
            .callback        = RAF_ApplicationJobModule,
            .arg             = &s_imu_context,
            .enabled_on_boot = true,
        },
        .required = false,
    },
    {
        .name     = "LCD",
        .init     = RAF_ApplicationInitLcd,
        .required = false,
    },
};

#define RAF_APP_MODULE_COUNT (sizeof(s_modules) / sizeof(s_modules[0]))

_Static_assert(
    RAF_APP_MODULE_COUNT == RAF_TASK_IDX_COUNT,
    "Jumlah module descriptor harus sama dengan RAF_TASK_IDX_COUNT"
);

static RAF_TaskId_t s_task_ids[RAF_TASK_IDX_COUNT];
static bool s_initialized = false;

static esp_err_t RAF_ApplicationInitIrqService(void) {
    static bool installed = false;
    if (installed) return ESP_OK;

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service gagal: %s", esp_err_to_name(err));
        return err;
    }

    installed = true;
    return ESP_OK;
}

static esp_err_t RAF_ApplicationRegisterModule(const RAF_AppModule_t *module,
                                               RAF_TaskId_t *out_task_id) {
    if (module == NULL || module->name == NULL) return ESP_ERR_INVALID_ARG;

    esp_err_t err;

    // 1) Init hardware
    if (module->init != NULL) {
        err = module->init();
        if (err != ESP_OK) {
            if (module->required) {
                ESP_LOGE(TAG, "[%s] init fatal: %s", module->name,
                         esp_err_to_name(err));
                return err;
            }

            ESP_LOGW(TAG, "[%s] init gagal, modul dilewati: %s", module->name,
                     esp_err_to_name(err));
            return ESP_OK;
        }
        ESP_LOGI(TAG, "[%s] init OK", module->name);
    }

    // 2) Register ISR (kalau ada)
    if (module->isr.enabled && module->isr.handler != NULL) {
        err = RAF_ApplicationInitIrqService();
        if (err != ESP_OK) return err;

        gpio_config_t gpio_cfg = {
            .pin_bit_mask = (1ULL << module->isr.pin),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = module->isr.trigger,
        };
        err = gpio_config(&gpio_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "[%s] gpio_config gagal: %s", module->name,
                     esp_err_to_name(err));
            return err;
        }

        err = gpio_isr_handler_add(module->isr.pin, module->isr.handler,
                                   module->isr.arg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "[%s] ISR add gagal: %s", module->name,
                     esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "[%s] ISR terpasang di GPIO %d", module->name,
                 module->isr.pin);
    }

    // 3) Register task (kalau ada)
    if (module->task.callback != NULL) {
        if (module->task.name == NULL || module->task.name[0] == '\0') {
            ESP_LOGE(TAG, "[%s] task.name kosong", module->name);
            return ESP_ERR_INVALID_ARG;
        }

        RAF_TaskConfig_t task_config = {
            .period_ms       = module->task.period_ms,
            .phase_ms        = module->task.phase_ms,
            .deadline_ms     = module->task.deadline_ms,
            .priority        = module->task.priority,
            .core            = module->task.core,
            .stack_size      = module->task.stack_size,
            .callback        = module->task.callback,
            .arg             = module->task.arg,
            .enabled_on_boot = module->task.enabled_on_boot,
        };

        strncpy(task_config.name, module->task.name,
                RAF_RT_TASK_NAME_MAX_LEN - 1);
        task_config.name[RAF_RT_TASK_NAME_MAX_LEN - 1] = '\0';

        err = RAF_SchedulerRegisterTask(&task_config, out_task_id);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "[%s] register task gagal: %s", module->name,
                     esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "[%s] task '%s' terdaftar", module->name,
                 module->task.name);
    }

    return ESP_OK;
}

// ============================================================================
// Public API
// ============================================================================
esp_err_t RAF_ApplicationInit(void) {
    if (s_initialized) {
        ESP_LOGW(TAG, "application sudah pernah diinisialisasi.");
        return ESP_OK;
    }

    // ---- Infrastruktur sistem ----
    esp_err_t err = ringbuf_com_init(RINGBUF_COMM_DEFAULT_SIZE);
    if (err != ESP_OK) return err;

    err = com_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "COM Hub gagal diinisialisasi: %s", esp_err_to_name(err));
    }

    iot_response_init();

    err = com_wifi_init(WIFI_SSID_DEFAULT, WIFI_PASS_DEFAULT);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi gagal diinisialisasi, sistem tetap berjalan: %s",
                 esp_err_to_name(err));
    }

    // ---- Reset task IDs ----
    for (size_t i = 0; i < RAF_TASK_IDX_COUNT; i++) {
        s_task_ids[i] = RAF_RT_INVALID_TASK_ID;
    }

    // ---- Loop semua modul ----
    ESP_LOGI(TAG, "Registering %zu application module(s)...",
             RAF_APP_MODULE_COUNT);
    for (size_t i = 0; i < RAF_APP_MODULE_COUNT; i++) {
        err = RAF_ApplicationRegisterModule(&s_modules[i], &s_task_ids[i]);
        if (err != ESP_OK) {
            if (s_modules[i].required) return err;
            ESP_LOGW(TAG, "Modul opsional [%s] tidak aktif", s_modules[i].name);
        }
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Application siap. %zu module descriptor diproses.",
             RAF_APP_MODULE_COUNT);
    return ESP_OK;
}

RAF_TaskId_t RAF_TaskRegistryGetId(RAF_TaskIndex_t idx) {
    if (idx >= RAF_TASK_IDX_COUNT) return RAF_RT_INVALID_TASK_ID;
    return s_task_ids[idx];
}