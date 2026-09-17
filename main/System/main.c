/*
 * main.c
 *
 *  Created on: 4 Sept 2026
 *      Author: Rafdi
 */

#include "main.h"
#include "task.h"
#include "com_can.h"

static rt_task_id_t s_com_task_id   = RT_INVALID_TASK_ID;
static rt_task_id_t s_diag_task_id  = RT_INVALID_TASK_ID;

static void task_com_hub_cb(void *arg) {
    com_update_1ms();
    com_can_rx_poll();
}

static void task_diagnostics_cb(void *arg) {
    ringbuf_com_print_stats();
    rt_scheduler_print_stats();
}

void app_main(void) {
    ESP_ERROR_CHECK(ringbuf_com_init(RINGBUF_COMM_DEFAULT_SIZE));
    startup_application();

    ESP_ERROR_CHECK(init_scheduler_engine());

    rt_task_config_t com_cfg = {
        .name = "COM_Dispatch",
        .period_ms = 1,
        .phase_ms = 0,
        .deadline_ms = 1,
        .priority = RT_PRIO_REALTIME,
        .core = RT_CORE_1,
        .stack_size = 4096,
        .callback = task_com_hub_cb,
        .enabled_on_boot = true
    };
    ESP_ERROR_CHECK(rt_scheduler_register_task(&com_cfg, &s_com_task_id));

    rt_task_config_t diag_cfg = {
        .name = "SYS_Diag",
        .period_ms = 1000,
        .phase_ms = 100,
        .deadline_ms = 0,   
        .priority = RT_PRIO_BACKGROUND,
        .core = RT_CORE_0,
        .stack_size = 3072,
        .callback = task_diagnostics_cb,
        .enabled_on_boot = true
    };
    ESP_ERROR_CHECK(rt_scheduler_register_task(&diag_cfg, &s_diag_task_id));

    ESP_ERROR_CHECK(rt_scheduler_start());
}
