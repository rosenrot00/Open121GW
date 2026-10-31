#include "dual_secondary_task.h"

#define DUAL_SECONDARY_TASK_TOGGLE_MS 3005u

static bool g_dual_secondary_active;
static bool g_dual_secondary_bank1;
static bool g_dual_secondary_changed;
static uint32_t g_dual_secondary_toggle_at;

void dual_secondary_task_init(uint32_t now_ms) {
    g_dual_secondary_active = false;
    g_dual_secondary_bank1 = false;
    g_dual_secondary_changed = false;
    g_dual_secondary_toggle_at = now_ms + DUAL_SECONDARY_TASK_TOGGLE_MS;
}

void dual_secondary_task_set_active(bool active, uint32_t now_ms) {
    if (g_dual_secondary_active == active) {
        return;
    }
    g_dual_secondary_active = active;
    g_dual_secondary_bank1 = false;
    g_dual_secondary_changed = true;
    g_dual_secondary_toggle_at = now_ms + DUAL_SECONDARY_TASK_TOGGLE_MS;
}

void dual_secondary_task_poll(uint32_t now_ms) {
    if (!g_dual_secondary_active) {
        return;
    }
    if ((int32_t)(now_ms - g_dual_secondary_toggle_at) < 0) {
        return;
    }
    g_dual_secondary_bank1 = !g_dual_secondary_bank1;
    g_dual_secondary_changed = true;
    g_dual_secondary_toggle_at = now_ms + DUAL_SECONDARY_TASK_TOGGLE_MS;
}

bool dual_secondary_task_bank1(void) {
    return g_dual_secondary_bank1;
}

bool dual_secondary_task_take_changed(void) {
    bool changed = g_dual_secondary_changed;
    g_dual_secondary_changed = false;
    return changed;
}
