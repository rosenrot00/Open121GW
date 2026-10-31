#include "sd_card_task.h"

#define SD_CARD_TASK_HEADER_MAX 112u
#define SD_CARD_TASK_LINE_MAX 176u
#define SD_CARD_TASK_QUEUE_CAPACITY 4u

typedef struct {
    char name83[11];
    char header[SD_CARD_TASK_HEADER_MAX];
    char line[SD_CARD_TASK_LINE_MAX];
} sd_card_task_entry_t;

static sd_card_task_entry_t g_queue[SD_CARD_TASK_QUEUE_CAPACITY];
static uint8_t g_queue_head;
static uint8_t g_queue_tail;
static uint8_t g_queue_count;
static bool g_io_window;
static bool g_error_pending;
static sd_fat_result_t g_last_result;

static void copy_name83(char dst[11], const char src[11]) {
    for (uint8_t i = 0; i < 11u; ++i) {
        dst[i] = src[i];
    }
}

static bool copy_cstr_bounded(char *dst, uint32_t cap, const char *src) {
    if (dst == 0 || cap == 0u) {
        return false;
    }
    if (src == 0) {
        dst[0] = '\0';
        return true;
    }
    uint32_t i = 0;
    while (src[i] != '\0') {
        if (i + 1u >= cap) {
            dst[0] = '\0';
            return false;
        }
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
    return true;
}

void sd_card_task_init(void) {
    g_queue_head = 0u;
    g_queue_tail = 0u;
    g_queue_count = 0u;
    g_io_window = false;
    g_error_pending = false;
    g_last_result = SD_FAT_OK;
}

void sd_card_task_poll(uint32_t now_ms) {
    (void)now_ms;
    if (g_queue_count == 0u || !g_io_window) {
        return;
    }

    g_io_window = false;
    sd_card_task_entry_t *entry = &g_queue[g_queue_tail];
    g_last_result = sd_fat_append_named_83_with_header(
        entry->name83, entry->header[0] != '\0' ? entry->header : 0, entry->line);
    g_queue_tail = (uint8_t)((g_queue_tail + 1u) % SD_CARD_TASK_QUEUE_CAPACITY);
    --g_queue_count;
    if (g_last_result != SD_FAT_OK) {
        g_error_pending = true;
        g_queue_head = 0u;
        g_queue_tail = 0u;
        g_queue_count = 0u;
    }
}

void sd_card_task_notify_measurement_boundary(void) {
    if (g_queue_count != 0u) {
        g_io_window = true;
    }
}

bool sd_card_task_append_named_83_with_header(const char name83[11],
                                              const char *header,
                                              const char *line) {
    if (name83 == 0 || line == 0 || g_queue_count >= SD_CARD_TASK_QUEUE_CAPACITY) {
        return false;
    }

    sd_card_task_entry_t *entry = &g_queue[g_queue_head];
    copy_name83(entry->name83, name83);
    if (!copy_cstr_bounded(entry->header, sizeof(entry->header), header) ||
        !copy_cstr_bounded(entry->line, sizeof(entry->line), line)) {
        return false;
    }
    g_queue_head = (uint8_t)((g_queue_head + 1u) % SD_CARD_TASK_QUEUE_CAPACITY);
    ++g_queue_count;
    return true;
}

bool sd_card_task_busy(void) {
    return g_queue_count != 0u;
}

uint8_t sd_card_task_queue_depth(void) {
    return g_queue_count;
}

bool sd_card_task_error_pending(void) {
    return g_error_pending;
}

sd_fat_result_t sd_card_task_last_result(void) {
    return g_last_result;
}

void sd_card_task_clear_error(void) {
    g_error_pending = false;
    g_last_result = SD_FAT_OK;
}
