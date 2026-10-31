#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sd_card_task.h"

static uint32_t g_append_count;
static sd_fat_result_t g_append_result;
static char g_last_line[32];

sd_fat_result_t sd_fat_append_named_83_with_header(const char name83[11],
                                                   const char *header,
                                                   const char *line) {
    (void)name83;
    (void)header;
    ++g_append_count;
    (void)strncpy(g_last_line, line, sizeof(g_last_line) - 1u);
    g_last_line[sizeof(g_last_line) - 1u] = '\0';
    return g_append_result;
}

static void reset_fixture(void) {
    g_append_count = 0u;
    g_append_result = SD_FAT_OK;
    g_last_line[0] = '\0';
    sd_card_task_init();
}

static void test_io_waits_for_measurement_boundary(void) {
    static const char name83[11] = {'E','E','V','L','O','G',' ',' ','C','S','V'};
    reset_fixture();

    assert(sd_card_task_append_named_83_with_header(name83, "head", "one"));
    sd_card_task_poll(1u);
    assert(g_append_count == 0u);
    assert(sd_card_task_queue_depth() == 1u);

    sd_card_task_notify_measurement_boundary();
    sd_card_task_poll(2u);
    assert(g_append_count == 1u);
    assert(strcmp(g_last_line, "one") == 0);
    assert(!sd_card_task_busy());
}

static void test_queue_preserves_order_and_applies_one_write_per_boundary(void) {
    static const char name83[11] = {'E','E','V','L','O','G',' ',' ','C','S','V'};
    reset_fixture();

    assert(sd_card_task_append_named_83_with_header(name83, 0, "one"));
    assert(sd_card_task_append_named_83_with_header(name83, 0, "two"));
    assert(sd_card_task_append_named_83_with_header(name83, 0, "three"));
    assert(sd_card_task_append_named_83_with_header(name83, 0, "four"));
    assert(!sd_card_task_append_named_83_with_header(name83, 0, "full"));

    sd_card_task_notify_measurement_boundary();
    sd_card_task_poll(1u);
    assert(g_append_count == 1u);
    assert(strcmp(g_last_line, "one") == 0);
    assert(sd_card_task_queue_depth() == 3u);

    sd_card_task_poll(2u);
    assert(g_append_count == 1u);

    sd_card_task_notify_measurement_boundary();
    sd_card_task_poll(3u);
    assert(g_append_count == 2u);
    assert(strcmp(g_last_line, "two") == 0);
}

static void test_write_error_stops_queued_io(void) {
    static const char name83[11] = {'E','E','V','L','O','G',' ',' ','C','S','V'};
    reset_fixture();
    g_append_result = SD_FAT_ERR_IO;

    assert(sd_card_task_append_named_83_with_header(name83, 0, "one"));
    assert(sd_card_task_append_named_83_with_header(name83, 0, "two"));
    sd_card_task_notify_measurement_boundary();
    sd_card_task_poll(1u);

    assert(sd_card_task_error_pending());
    assert(sd_card_task_last_result() == SD_FAT_ERR_IO);
    assert(sd_card_task_queue_depth() == 0u);
    sd_card_task_clear_error();
    assert(!sd_card_task_error_pending());
}

int main(void) {
    test_io_waits_for_measurement_boundary();
    test_queue_preserves_order_and_applies_one_write_per_boundary();
    test_write_error_stops_queued_io();
    return 0;
}
