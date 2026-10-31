#include "display_task.h"

static uint32_t g_display_next_update;
static bool g_display_update_due;
static uint32_t g_bargraph_next_update;
static bool g_bargraph_update_due;

void display_task_init(uint32_t now_ms) {
    g_display_next_update = now_ms;
    g_display_update_due = true;
    g_bargraph_next_update = now_ms;
    g_bargraph_update_due = true;
}

bool display_task_update_due(uint32_t now_ms) {
    return g_display_update_due ||
           (int32_t)(now_ms - g_display_next_update) >= 0;
}

void display_task_request_now(void) {
    g_display_update_due = true;
}

void display_task_schedule_after(uint32_t now_ms, uint32_t delay_ms) {
    g_display_next_update = now_ms + delay_ms;
    g_display_update_due = delay_ms == 0u;
}

bool display_task_bargraph_update_due(uint32_t now_ms) {
    return g_bargraph_update_due ||
           (int32_t)(now_ms - g_bargraph_next_update) >= 0;
}

void display_task_schedule_bargraph_after(uint32_t now_ms, uint32_t delay_ms) {
    g_bargraph_next_update = now_ms + delay_ms;
    g_bargraph_update_due = delay_ms == 0u;
}
