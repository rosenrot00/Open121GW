#include "p10_measure_task.h"

static bool g_p10_task_active;
static bool g_p10_task_pending;
static dmm_reading_t g_p10_task_reading;
static p10_measure_task_diag_t g_p10_task_diag;

static bool p10_measure_task_should_replace_pending(const dmm_reading_t *pending,
                                                    const dmm_reading_t *next) {
    if (pending->mode == DMM_MODE_P10_CAP && next->mode == DMM_MODE_P10_CAP &&
        pending->display_ready && !next->display_ready) {
        return false;
    }
    return true;
}

void p10_measure_task_init(void) {
    g_p10_task_active = false;
    g_p10_task_pending = false;
    g_p10_task_diag.active = false;
    g_p10_task_diag.pending = false;
    g_p10_task_diag.poll_count = 0;
    g_p10_task_diag.event_count = 0;
    g_p10_task_diag.sample_count = 0;
    g_p10_task_diag.range_change_count = 0;
    g_p10_task_diag.overwrite_count = 0;
    g_p10_task_diag.last_mode = 0xFFu;
    g_p10_task_diag.last_range = 0xFFu;
}

void p10_measure_task_set_active(bool active) {
    if (g_p10_task_active == active) {
        return;
    }

    g_p10_task_active = active;
    g_p10_task_diag.active = active;
    p10_measure_task_flush();
}

bool p10_measure_task_active(void) {
    return g_p10_task_active;
}

void p10_measure_task_flush(void) {
    g_p10_task_pending = false;
    g_p10_task_diag.pending = false;
}

void p10_measure_task_poll(uint32_t now_ms) {
    (void)now_ms;
    if (!g_p10_task_active) {
        return;
    }

    ++g_p10_task_diag.poll_count;
    dmm_reading_t reading;
    /*
     * The historical name is P10, but dmm_p10_poll() is the shared stock
     * frontend pipeline for all implemented rotary measurement modes,
     * including Low-Z.
     */
    if (!dmm_p10_poll(&reading)) {
        return;
    }

    ++g_p10_task_diag.event_count;
    if (reading.has_sample) {
        ++g_p10_task_diag.sample_count;
    }
    if (reading.range_changed) {
        ++g_p10_task_diag.range_change_count;
    }
    if (g_p10_task_pending) {
        ++g_p10_task_diag.overwrite_count;
        if (!p10_measure_task_should_replace_pending(&g_p10_task_reading, &reading)) {
            return;
        }
    }
    g_p10_task_diag.last_mode = reading.mode;
    g_p10_task_diag.last_range = reading.range;
    g_p10_task_reading = reading;
    g_p10_task_pending = true;
    g_p10_task_diag.pending = true;
}

bool p10_measure_task_take_reading(dmm_reading_t *reading) {
    if (reading == 0 || !g_p10_task_pending) {
        return false;
    }

    *reading = g_p10_task_reading;
    g_p10_task_pending = false;
    g_p10_task_diag.pending = false;
    return true;
}

void p10_measure_task_get_diag(p10_measure_task_diag_t *diag) {
    if (diag == 0) {
        return;
    }
    *diag = g_p10_task_diag;
    diag->active = g_p10_task_active;
    diag->pending = g_p10_task_pending;
}
