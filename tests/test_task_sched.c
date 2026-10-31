#include "task_sched.h"

#include <assert.h>
#include <stdint.h>

static uint32_t g_clock_ms;
static uint32_t g_callback_runtime_ms;
static uint32_t g_callback_count;
static uint32_t g_callback_now_ms;
static bool g_use_slow_first_task;

uint32_t clock_millis(void) {
    return g_clock_ms;
}

static void measured_task(uint32_t now_ms, void *ctx) {
    (void)ctx;
    ++g_callback_count;
    g_callback_now_ms = now_ms;
    g_clock_ms += g_callback_runtime_ms;
}

static void slow_first_task(uint32_t now_ms, void *ctx) {
    (void)now_ms;
    (void)ctx;
    if (g_use_slow_first_task) {
        g_clock_ms += 7u;
    }
}

int main(void) {
    task_sched_init();
    task_sched_handle_t handle = task_sched_add("measured", measured_task, 0, 10u, 10u);
    assert(handle != TASK_SCHED_INVALID_HANDLE);

    task_sched_run_due(9u);
    assert(g_callback_count == 0u);

    g_clock_ms = 10u;
    g_callback_runtime_ms = 3u;
    task_sched_run_due(10u);

    const task_sched_task_t *task = task_sched_get(handle);
    assert(task != 0);
    assert(task->run_count == 1u);
    assert(task->late_run_count == 0u);
    assert(task->skipped_run_count == 0u);
    assert(task->overrun_count == 0u);
    assert(task->total_runtime_ms == 3u);
    assert(task->last_runtime_ms == 3u);
    assert(task->max_runtime_ms == 3u);
    assert(task->max_late_ms == 0u);
    assert(task->next_due_ms == 20u);

    g_clock_ms = 35u;
    g_callback_runtime_ms = 12u;
    task_sched_run_due(35u);

    task = task_sched_get(handle);
    assert(task->run_count == 2u);
    assert(task->late_run_count == 1u);
    assert(task->skipped_run_count == 1u);
    assert(task->overrun_count == 1u);
    assert(task->total_runtime_ms == 15u);
    assert(task->last_runtime_ms == 12u);
    assert(task->max_runtime_ms == 12u);
    assert(task->max_late_ms == 15u);
    assert(task->next_due_ms == 40u);

    task_sched_init();
    g_clock_ms = 10u;
    g_callback_runtime_ms = 0u;
    g_callback_count = 0u;
    g_use_slow_first_task = true;
    assert(task_sched_add("slow", slow_first_task, 0, 10u, 10u) !=
           TASK_SCHED_INVALID_HANDLE);
    handle = task_sched_add("after", measured_task, 0, 10u, 10u);
    assert(handle != TASK_SCHED_INVALID_HANDLE);
    task_sched_run_due(10u);
    task = task_sched_get(handle);
    assert(task != 0);
    assert(task->run_count == 1u);
    assert(task->late_run_count == 1u);
    assert(task->max_late_ms == 7u);

    task_sched_init();
    g_clock_ms = 10u;
    g_callback_runtime_ms = 0u;
    g_callback_count = 0u;
    g_callback_now_ms = 0u;
    g_use_slow_first_task = true;
    assert(task_sched_add("slow", slow_first_task, 0, 10u, 10u) !=
           TASK_SCHED_INVALID_HANDLE);
    handle = task_sched_add("becomes_due", measured_task, 0, 10u, 15u);
    assert(handle != TASK_SCHED_INVALID_HANDLE);
    task_sched_run_due(10u);
    task = task_sched_get(handle);
    assert(task != 0);
    assert(task->run_count == 1u);
    assert(task->late_run_count == 1u);
    assert(task->max_late_ms == 2u);
    assert(g_callback_count == 1u);
    assert(g_callback_now_ms == 17u);

    task_sched_request_stats_reset();
    g_clock_ms = 25u;
    g_use_slow_first_task = false;
    task_sched_run_due(25u);
    task = task_sched_get(handle);
    assert(task != 0);
    assert(task->run_count == 1u);
    assert(task->late_run_count == 0u);
    assert(task->skipped_run_count == 0u);
    assert(task->overrun_count == 0u);
    assert(task->total_runtime_ms == 0u);
    assert(task->max_late_ms == 0u);
    return 0;
}
