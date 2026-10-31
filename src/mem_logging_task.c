#include "mem_logging_task.h"

static bool g_mem_logging_active;
static bool g_mem_logging_icon_gate;
static uint32_t g_mem_logging_next_blink;
static uint32_t g_mem_logging_next_sample;
static uint32_t g_mem_logging_sample;
static char g_mem_logging_name83[11];
static bool g_mem_logging_sample_due;

static void mem_logging_copy_name83(const char src[11]) {
    for (uint8_t i = 0; i < 11u; ++i) {
        g_mem_logging_name83[i] = src[i];
    }
}

void mem_logging_task_init(uint32_t now_ms) {
    static const char default_name[11] = {'E','E','V','L','O','G',' ',' ','C','S','V'};
    g_mem_logging_active = false;
    g_mem_logging_icon_gate = false;
    g_mem_logging_next_blink = now_ms;
    g_mem_logging_next_sample = now_ms;
    g_mem_logging_sample = 0;
    g_mem_logging_sample_due = false;
    mem_logging_copy_name83(default_name);
}

void mem_logging_task_poll(uint32_t now_ms) {
    if (g_mem_logging_active && !g_mem_logging_sample_due &&
        (int32_t)(now_ms - g_mem_logging_next_sample) >= 0) {
        g_mem_logging_sample_due = true;
    }
    if ((g_mem_logging_active || g_mem_logging_icon_gate) &&
        (int32_t)(now_ms - g_mem_logging_next_blink) >= 0) {
        g_mem_logging_icon_gate = !g_mem_logging_icon_gate;
        g_mem_logging_next_blink = now_ms + 500u;
    }
}

void mem_logging_task_start(const char name83[11], uint32_t now_ms) {
    mem_logging_copy_name83(name83);
    g_mem_logging_active = true;
    g_mem_logging_icon_gate = true;
    g_mem_logging_next_blink = now_ms + 500u;
    g_mem_logging_next_sample = now_ms;
    g_mem_logging_sample = 1u;
    g_mem_logging_sample_due = false;
}

void mem_logging_task_stop(uint32_t now_ms) {
    g_mem_logging_active = false;
    g_mem_logging_icon_gate = true;
    g_mem_logging_next_blink = now_ms + 500u;
    g_mem_logging_sample_due = false;
}

void mem_logging_task_clear_icon_gate(void) {
    if (!g_mem_logging_active) {
        g_mem_logging_icon_gate = false;
    }
}

void mem_logging_task_finish_sample(bool queued, uint16_t interval_s, uint32_t now_ms) {
    g_mem_logging_sample_due = false;
    if (!g_mem_logging_active) {
        return;
    }
    if (!queued) {
        mem_logging_task_stop(now_ms);
        return;
    }
    if (g_mem_logging_sample < 99999999u) {
        ++g_mem_logging_sample;
    }
    uint16_t interval = interval_s != 0u ? interval_s : 1u;
    g_mem_logging_next_sample = now_ms + (uint32_t)interval * 1000u;
}

bool mem_logging_task_take_event(mem_logging_event_t *event) {
    if (event == 0) {
        return false;
    }
    if (g_mem_logging_active && g_mem_logging_sample_due) {
        *event = MEM_LOGGING_EVENT_SAMPLE_DUE;
        return true;
    }
    *event = MEM_LOGGING_EVENT_NONE;
    return false;
}

bool mem_logging_task_active(void) {
    return g_mem_logging_active;
}

bool mem_logging_task_icon_gate(void) {
    return g_mem_logging_icon_gate;
}

uint32_t mem_logging_task_sample(void) {
    return g_mem_logging_sample;
}

const char *mem_logging_task_name83(void) {
    return g_mem_logging_name83;
}
