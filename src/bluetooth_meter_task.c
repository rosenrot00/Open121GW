#include "bluetooth_meter_task.h"

#define BLUETOOTH_METER_TASK_FRAME_INTERVAL_MS 66u

static bool g_bt_meter_active;
static bool g_bt_meter_frame_due;
static uint32_t g_bt_meter_next_frame;

void bluetooth_meter_task_init(uint32_t now_ms) {
    g_bt_meter_active = false;
    g_bt_meter_frame_due = false;
    g_bt_meter_next_frame = now_ms;
}

void bluetooth_meter_task_set_active(bool active, uint32_t now_ms) {
    if (g_bt_meter_active == active) {
        return;
    }
    g_bt_meter_active = active;
    g_bt_meter_frame_due = active;
    g_bt_meter_next_frame = now_ms;
}

void bluetooth_meter_task_poll(uint32_t now_ms) {
    if (!g_bt_meter_active || g_bt_meter_frame_due) {
        return;
    }
    if ((int32_t)(now_ms - g_bt_meter_next_frame) >= 0) {
        g_bt_meter_frame_due = true;
    }
}

bool bluetooth_meter_task_take_frame_due(void) {
    if (!g_bt_meter_active || !g_bt_meter_frame_due) {
        return false;
    }
    g_bt_meter_frame_due = false;
    return true;
}

void bluetooth_meter_task_frame_sent(uint32_t now_ms) {
    g_bt_meter_next_frame = now_ms + BLUETOOTH_METER_TASK_FRAME_INTERVAL_MS;
}
