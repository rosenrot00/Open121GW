#include "status_task.h"

static char g_status_text[5];
static uint32_t g_status_until;
static bool g_status_active;
static bool g_status_expired;

static void status_task_copy5(const char src[5]) {
    for (uint8_t i = 0; i < 5u; ++i) {
        g_status_text[i] = src[i];
    }
}

void status_task_init(void) {
    static const char blank[5] = {' ', ' ', ' ', ' ', ' '};
    status_task_copy5(blank);
    g_status_until = 0;
    g_status_active = false;
    g_status_expired = false;
}

void status_task_poll(uint32_t now_ms) {
    if (g_status_active && (int32_t)(now_ms - g_status_until) >= 0) {
        g_status_active = false;
        g_status_until = 0;
        g_status_expired = true;
    }
}

void status_task_show_text(const char text[5], uint32_t now_ms, uint32_t duration_ms) {
    status_task_copy5(text);
    g_status_until = now_ms + duration_ms;
    g_status_active = duration_ms != 0u;
    g_status_expired = false;
}

bool status_task_active(void) {
    return g_status_active;
}

bool status_task_take_expired(void) {
    bool expired = g_status_expired;
    g_status_expired = false;
    return expired;
}

const char *status_task_text(void) {
    return g_status_text;
}
