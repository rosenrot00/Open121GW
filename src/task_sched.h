#ifndef EEVBLOG_TASK_SCHED_H
#define EEVBLOG_TASK_SCHED_H

#include <stdbool.h>
#include <stdint.h>

#define TASK_SCHED_MAX_TASKS 16u
#define TASK_SCHED_INVALID_HANDLE 0xFFu

typedef uint8_t task_sched_handle_t;
typedef void (*task_sched_fn_t)(uint32_t now_ms, void *ctx);

typedef struct {
    const char *name;
    task_sched_fn_t run;
    void *ctx;
    uint32_t period_ms;
    uint32_t next_due_ms;
    uint32_t run_count;
    uint32_t late_run_count;
    uint32_t skipped_run_count;
    uint32_t overrun_count;
    uint32_t total_runtime_ms;
    uint32_t last_runtime_ms;
    uint32_t max_runtime_ms;
    uint32_t max_late_ms;
    bool enabled;
} task_sched_task_t;

void task_sched_init(void);
task_sched_handle_t task_sched_add(const char *name, task_sched_fn_t run, void *ctx,
                                   uint32_t period_ms, uint32_t first_due_ms);
void task_sched_run_due(uint32_t now_ms);
void task_sched_request_stats_reset(void);
bool task_sched_set_enabled(task_sched_handle_t handle, bool enabled, uint32_t now_ms);
const task_sched_task_t *task_sched_get(task_sched_handle_t handle);
uint8_t task_sched_count(void);

#endif
