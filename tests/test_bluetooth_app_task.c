#include "bluetooth_app_task.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define TEST_BUFFER_SIZE 32u

static uint8_t g_rx[TEST_BUFFER_SIZE];
static uint8_t g_rx_head;
static uint8_t g_rx_tail;
static bool g_tx_accept;
static uint8_t g_tx[TEST_BUFFER_SIZE];
static uint8_t g_tx_len;
static uint8_t g_tx_attempts;

static void feed_rx(const uint8_t *data, uint8_t len) {
    assert((uint8_t)(g_rx_head + len) <= TEST_BUFFER_SIZE);
    memcpy(&g_rx[g_rx_head], data, len);
    g_rx_head = (uint8_t)(g_rx_head + len);
}

bool bluetooth_receive_byte(uint8_t *byte) {
    if (g_rx_tail == g_rx_head) {
        return false;
    }
    *byte = g_rx[g_rx_tail++];
    return true;
}

bool bluetooth_send_bytes_irq(const uint8_t *data, uint8_t len) {
    ++g_tx_attempts;
    if (!g_tx_accept) {
        return false;
    }
    assert(len <= TEST_BUFFER_SIZE);
    memcpy(g_tx, data, len);
    g_tx_len = len;
    return true;
}

bool uart_update_rx_byte(uint8_t byte) {
    (void)byte;
    return false;
}

bool uart_update_bt_stream_enabled(void) {
    return true;
}

int main(void) {
    static const uint8_t range_frame[] = {0xF4u, '0', '1', '0', '1'};
    bluetooth_app_event_t event;

    bluetooth_app_task_init();
    feed_rx(range_frame, sizeof(range_frame));

    g_tx_accept = false;
    bluetooth_app_task_service();
    assert(g_tx_attempts == 1u);
    assert(g_tx_len == 0u);
    assert(bluetooth_app_task_take_event(&event));
    assert(event.kind == BLUETOOTH_APP_EVENT_BUTTON);
    assert(event.value == 0x10u);

    g_tx_accept = true;
    bluetooth_app_task_service();
    assert(g_tx_attempts == 2u);
    assert(g_tx_len == sizeof(range_frame));
    assert(memcmp(g_tx, range_frame, sizeof(range_frame)) == 0);

    bluetooth_app_task_service();
    assert(g_tx_attempts == 2u);
    assert(!bluetooth_app_task_take_event(&event));
    return 0;
}
