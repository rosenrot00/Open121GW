#include "apo_task.h"

#define APO_TASK_NORMAL_SECONDS 1800u
#define APO_TASK_MINMAX_SECONDS 3600u
#define APO_TASK_WARNING_SECONDS 60u
#define APO_TASK_WARNING_BEEPS 5u

static bool g_apo_enabled;
static bool g_apo_suspended;
static uint16_t g_apo_seconds_remaining;
static uint8_t g_apo_warning_beeps_remaining;
static uint32_t g_apo_last_second_tick;
static uint8_t g_apo_pending_warning_beeps;
static bool g_apo_pending_timeout;

void apo_task_init(uint32_t now_ms) {
    g_apo_enabled = true;
    g_apo_suspended = false;
    g_apo_seconds_remaining = APO_TASK_NORMAL_SECONDS;
    g_apo_warning_beeps_remaining = 0;
    g_apo_last_second_tick = now_ms;
    g_apo_pending_warning_beeps = 0;
    g_apo_pending_timeout = false;
}

void apo_task_set_enabled(bool enabled, uint32_t now_ms) {
    if (g_apo_enabled == enabled) {
        return;
    }
    g_apo_enabled = enabled;
    if (!enabled) {
        g_apo_seconds_remaining = APO_TASK_NORMAL_SECONDS;
        g_apo_warning_beeps_remaining = 0;
        g_apo_pending_warning_beeps = 0;
        g_apo_pending_timeout = false;
    }
    g_apo_last_second_tick = now_ms;
}

void apo_task_set_suspended(bool suspended, uint32_t now_ms) {
    g_apo_suspended = suspended;
    if (suspended) {
        g_apo_last_second_tick = now_ms;
    }
}

void apo_task_reset(bool minmax_active, uint32_t now_ms) {
    g_apo_seconds_remaining = minmax_active ? APO_TASK_MINMAX_SECONDS : APO_TASK_NORMAL_SECONDS;
    g_apo_warning_beeps_remaining = 0;
    g_apo_pending_warning_beeps = 0;
    g_apo_pending_timeout = false;
    g_apo_last_second_tick = now_ms;
}

void apo_task_poll(uint32_t now_ms) {
    if (!g_apo_enabled || g_apo_suspended || g_apo_pending_timeout) {
        return;
    }
    if ((uint32_t)(now_ms - g_apo_last_second_tick) < 1000u) {
        return;
    }

    do {
        g_apo_last_second_tick += 1000u;
        if (g_apo_seconds_remaining != 0u) {
            --g_apo_seconds_remaining;
            if (g_apo_seconds_remaining == APO_TASK_WARNING_SECONDS) {
                g_apo_warning_beeps_remaining = APO_TASK_WARNING_BEEPS;
            }
            if (g_apo_warning_beeps_remaining != 0u) {
                ++g_apo_pending_warning_beeps;
                --g_apo_warning_beeps_remaining;
            }
        }
    } while ((uint32_t)(now_ms - g_apo_last_second_tick) >= 1000u);

    if (g_apo_seconds_remaining == 0u) {
        g_apo_pending_timeout = true;
    }
}

bool apo_task_take_event(apo_task_event_t *event) {
    if (event == 0) {
        return false;
    }
    if (g_apo_pending_warning_beeps != 0u) {
        --g_apo_pending_warning_beeps;
        *event = APO_TASK_EVENT_WARNING_BEEP;
        return true;
    }
    if (g_apo_pending_timeout) {
        g_apo_pending_timeout = false;
        *event = APO_TASK_EVENT_TIMEOUT;
        return true;
    }
    *event = APO_TASK_EVENT_NONE;
    return false;
}

uint16_t apo_task_seconds_remaining(void) {
    return g_apo_seconds_remaining;
}
