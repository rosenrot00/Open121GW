#include "task_sched.h"

#include "clock.h"

static task_sched_task_t g_tasks[TASK_SCHED_MAX_TASKS];
static uint8_t g_task_count;
static bool g_stats_reset_pending;

static void task_sched_clear_stats(task_sched_task_t *task) {
    task->run_count = 0u;
    task->late_run_count = 0u;
    task->skipped_run_count = 0u;
    task->overrun_count = 0u;
    task->total_runtime_ms = 0u;
    task->last_runtime_ms = 0u;
    task->max_runtime_ms = 0u;
    task->max_late_ms = 0u;
}

void task_sched_init(void) {
    g_task_count = 0;
    g_stats_reset_pending = false;
    for (uint8_t i = 0; i < TASK_SCHED_MAX_TASKS; ++i) {
        g_tasks[i].name = 0;
        g_tasks[i].run = 0;
        g_tasks[i].ctx = 0;
        g_tasks[i].period_ms = 0;
        g_tasks[i].next_due_ms = 0;
        task_sched_clear_stats(&g_tasks[i]);
        g_tasks[i].enabled = false;
    }
}

task_sched_handle_t task_sched_add(const char *name, task_sched_fn_t run, void *ctx,
                                   uint32_t period_ms, uint32_t first_due_ms) {
    if (run == 0 || period_ms == 0u || g_task_count >= TASK_SCHED_MAX_TASKS) {
        return TASK_SCHED_INVALID_HANDLE;
    }

    task_sched_handle_t handle = g_task_count;
    g_tasks[handle].name = name;
    g_tasks[handle].run = run;
    g_tasks[handle].ctx = ctx;
    g_tasks[handle].period_ms = period_ms;
    g_tasks[handle].next_due_ms = first_due_ms;
    task_sched_clear_stats(&g_tasks[handle]);
    g_tasks[handle].enabled = true;
    ++g_task_count;
    return handle;
}

void task_sched_run_due(uint32_t now_ms) {
    if (g_stats_reset_pending) {
        for (uint8_t i = 0u; i < g_task_count; ++i) {
            task_sched_clear_stats(&g_tasks[i]);
        }
        g_stats_reset_pending = false;
    }

    uint32_t scheduler_now_ms = now_ms;
    for (uint8_t i = 0; i < g_task_count; ++i) {
        task_sched_task_t *task = &g_tasks[i];
        if (!task->enabled || task->run == 0) {
            continue;
        }
        if ((int32_t)(scheduler_now_ms - task->next_due_ms) < 0) {
            continue;
        }

        uint32_t runtime_start_ms = clock_millis();
        int32_t late_delta_ms = (int32_t)(runtime_start_ms - task->next_due_ms);
        if (late_delta_ms > 0) {
            uint32_t late_ms = (uint32_t)late_delta_ms;
            ++task->late_run_count;
            if (late_ms > task->max_late_ms) {
                task->max_late_ms = late_ms;
            }
        }

        do {
            task->next_due_ms += task->period_ms;
            if ((int32_t)(runtime_start_ms - task->next_due_ms) >= 0) {
                ++task->skipped_run_count;
            }
        } while ((int32_t)(runtime_start_ms - task->next_due_ms) >= 0);

        task->run(runtime_start_ms, task->ctx);
        scheduler_now_ms = clock_millis();
        uint32_t runtime_ms = scheduler_now_ms - runtime_start_ms;
        ++task->run_count;
        task->total_runtime_ms += runtime_ms;
        task->last_runtime_ms = runtime_ms;
        if (runtime_ms > task->max_runtime_ms) {
            task->max_runtime_ms = runtime_ms;
        }
        if (runtime_ms > task->period_ms) {
            ++task->overrun_count;
        }
    }
}

void task_sched_request_stats_reset(void) {
    g_stats_reset_pending = true;
}

bool task_sched_set_enabled(task_sched_handle_t handle, bool enabled, uint32_t now_ms) {
    if (handle >= g_task_count) {
        return false;
    }
    g_tasks[handle].enabled = enabled;
    if (enabled) {
        g_tasks[handle].next_due_ms = now_ms + g_tasks[handle].period_ms;
    }
    return true;
}

const task_sched_task_t *task_sched_get(task_sched_handle_t handle) {
    if (handle >= g_task_count) {
        return 0;
    }
    return &g_tasks[handle];
}

uint8_t task_sched_count(void) {
    return g_task_count;
}
