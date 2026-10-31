#include "task_runtime.h"

#include "apo_task.h"
#include "aux_adc_task.h"
#include "bluetooth_app_task.h"
#include "bluetooth_meter_task.h"
#include "buzzer.h"
#include "display_task.h"
#include "dual_secondary_task.h"
#include "input_task.h"
#include "lead_warning_task.h"
#include "mem_logging_task.h"
#include "p10_measure_task.h"
#include "sd_card_task.h"
#include "status_task.h"
#include "task_sched.h"

static uint32_t g_heartbeat_count;

static void task_runtime_p10_measure(uint32_t now_ms, void *ctx) {
    (void)ctx;
    p10_measure_task_poll(now_ms);
}

static void task_runtime_aux_adc(uint32_t now_ms, void *ctx) {
    (void)ctx;
    aux_adc_task_poll(now_ms);
}

static void task_runtime_apo(uint32_t now_ms, void *ctx) {
    (void)ctx;
    apo_task_poll(now_ms);
}

static void task_runtime_input(uint32_t now_ms, void *ctx) {
    (void)ctx;
    input_task_poll(now_ms);
}

static void task_runtime_sd_card(uint32_t now_ms, void *ctx) {
    (void)ctx;
    sd_card_task_poll(now_ms);
}

static void task_runtime_mem_logging(uint32_t now_ms, void *ctx) {
    (void)ctx;
    mem_logging_task_poll(now_ms);
}

static void task_runtime_bluetooth_app(uint32_t now_ms, void *ctx) {
    (void)now_ms;
    (void)ctx;
    bluetooth_app_task_service();
}

static void task_runtime_bluetooth_meter(uint32_t now_ms, void *ctx) {
    (void)ctx;
    bluetooth_meter_task_poll(now_ms);
}

static void task_runtime_dual_secondary(uint32_t now_ms, void *ctx) {
    (void)ctx;
    dual_secondary_task_poll(now_ms);
}

static void task_runtime_lead_warning(uint32_t now_ms, void *ctx) {
    (void)ctx;
    lead_warning_task_poll(now_ms);
}

static void task_runtime_buzzer(uint32_t now_ms, void *ctx) {
    (void)now_ms;
    (void)ctx;
    buzzer_poll();
}

static void task_runtime_status(uint32_t now_ms, void *ctx) {
    (void)ctx;
    status_task_poll(now_ms);
}

static void task_runtime_heartbeat(uint32_t now_ms, void *ctx) {
    (void)now_ms;
    (void)ctx;
    ++g_heartbeat_count;
}

void task_runtime_init(uint32_t now_ms) {
    task_sched_init();
    apo_task_init(now_ms);
    display_task_init(now_ms);
    dual_secondary_task_init(now_ms);
    input_task_init(now_ms);
    lead_warning_task_init(now_ms);
    mem_logging_task_init(now_ms);
    p10_measure_task_init();
    aux_adc_task_init(now_ms);
    bluetooth_meter_task_init(now_ms);
    sd_card_task_init();
    status_task_init();
    g_heartbeat_count = 0;
    (void)task_sched_add("input", task_runtime_input, 0, 1u, now_ms + 1u);
    (void)task_sched_add("p10_measure", task_runtime_p10_measure, 0, 1u, now_ms + 1u);
    (void)task_sched_add("aux_adc", task_runtime_aux_adc, 0,
                         AUX_ADC_TASK_PERIOD_MS, now_ms + 1u);
    (void)task_sched_add("apo", task_runtime_apo, 0, 100u, now_ms + 100u);
    (void)task_sched_add("bt_app", task_runtime_bluetooth_app, 0, 1u, now_ms + 1u);
    (void)task_sched_add("bt_meter", task_runtime_bluetooth_meter, 0, 10u, now_ms + 10u);
    (void)task_sched_add("dual_sec", task_runtime_dual_secondary, 0, 20u, now_ms + 20u);
    (void)task_sched_add("lead_warn", task_runtime_lead_warning, 0, 1u, now_ms + 1u);
    (void)task_sched_add("buzzer", task_runtime_buzzer, 0, 1u, now_ms + 1u);
    (void)task_sched_add("mem_log", task_runtime_mem_logging, 0, 20u, now_ms + 20u);
    (void)task_sched_add("sd_card", task_runtime_sd_card, 0, 1u, now_ms + 1u);
    (void)task_sched_add("status", task_runtime_status, 0, 20u, now_ms + 20u);
    (void)task_sched_add("heartbeat", task_runtime_heartbeat, 0, 1000u, now_ms + 1000u);
}

void task_runtime_poll(uint32_t now_ms) {
    task_sched_run_due(now_ms);
}

uint32_t task_runtime_heartbeat_count(void) {
    return g_heartbeat_count;
}
