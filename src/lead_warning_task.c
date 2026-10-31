#include "lead_warning_task.h"

#include "buzzer.h"

#define LEAD_WARNING_TASK_BEEP_INTERVAL_MS 66u
#define LEAD_WARNING_TASK_BEEP_MS 65u

static bool g_lead_warning_active;
static uint32_t g_lead_warning_next_beep;

void lead_warning_task_init(uint32_t now_ms) {
    g_lead_warning_active = false;
    g_lead_warning_next_beep = now_ms;
}

void lead_warning_task_set_active(bool active, uint32_t now_ms) {
    if (g_lead_warning_active == active) {
        return;
    }
    g_lead_warning_active = active;
    g_lead_warning_next_beep = now_ms;
}

void lead_warning_task_poll(uint32_t now_ms) {
    if (!g_lead_warning_active) {
        g_lead_warning_next_beep = now_ms;
        return;
    }
    if ((int32_t)(now_ms - g_lead_warning_next_beep) < 0) {
        return;
    }
    buzzer_force_on_ms(LEAD_WARNING_TASK_BEEP_MS);
    g_lead_warning_next_beep = now_ms + LEAD_WARNING_TASK_BEEP_INTERVAL_MS;
}
