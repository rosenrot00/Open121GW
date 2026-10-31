#include "bluetooth_app_task.h"

#include "bluetooth.h"
#include "uart_update.h"

#define BT_APP_EVENT_QUEUE_SIZE 4u
#define BT_APP_MAX_ECHO_SIZE 15u

#define BT_APP_BUTTON_MODE 0x01u
#define BT_APP_BUTTON_SETUP 0x02u
#define BT_APP_BUTTON_MINMAX 0x04u
#define BT_APP_BUTTON_MEM 0x08u
#define BT_APP_BUTTON_RANGE 0x10u
#define BT_APP_BUTTON_HOLD 0x20u
#define BT_APP_BUTTON_REL 0x40u
#define BT_APP_BUTTON_DOWN 0x80u

typedef enum {
    BT_APP_RX_NONE = 0,
    BT_APP_RX_F4,
    BT_APP_RX_F8,
} bt_app_rx_kind_t;

static uint8_t g_rx_frame[15];
static uint8_t g_rx_len;
static bluetooth_app_event_t g_event_queue[BT_APP_EVENT_QUEUE_SIZE];
static uint8_t g_event_head;
static uint8_t g_event_tail;
static uint8_t g_event_count;
static uint8_t g_echo_frame[BT_APP_MAX_ECHO_SIZE];
static uint8_t g_echo_len;

static void clear_event(bluetooth_app_event_t *event) {
    event->kind = BLUETOOTH_APP_EVENT_NONE;
    event->value = 0u;
    for (uint8_t i = 0; i < sizeof(event->datetime); ++i) {
        event->datetime[i] = 0u;
    }
}

static void push_event(const bluetooth_app_event_t *event) {
    if (event == 0 || event->kind == BLUETOOTH_APP_EVENT_NONE) {
        return;
    }
    if (g_event_count >= BT_APP_EVENT_QUEUE_SIZE) {
        g_event_tail = (uint8_t)((g_event_tail + 1u) % BT_APP_EVENT_QUEUE_SIZE);
        --g_event_count;
    }
    g_event_queue[g_event_head] = *event;
    g_event_head = (uint8_t)((g_event_head + 1u) % BT_APP_EVENT_QUEUE_SIZE);
    ++g_event_count;
}

static int8_t hex_value(uint8_t ch) {
    if (ch >= (uint8_t)'0' && ch <= (uint8_t)'9') {
        return (int8_t)(ch - (uint8_t)'0');
    }
    if (ch >= (uint8_t)'A' && ch <= (uint8_t)'F') {
        return (int8_t)(ch - (uint8_t)'A' + 10u);
    }
    if (ch >= (uint8_t)'a' && ch <= (uint8_t)'f') {
        return (int8_t)(ch - (uint8_t)'a' + 10u);
    }
    return -1;
}

static bool decode_ascii_hex_byte(uint8_t hi, uint8_t lo, uint8_t *out) {
    int8_t h = hex_value(hi);
    int8_t l = hex_value(lo);
    if (h < 0 || l < 0) {
        return false;
    }
    *out = (uint8_t)(((uint8_t)h << 4) | (uint8_t)l);
    return true;
}

static bool decode_ascii_hex_at(const uint8_t *frame, uint8_t index, uint8_t *out) {
    return decode_ascii_hex_byte(frame[index], frame[index + 1u], out);
}

static uint8_t f4_command_to_button_mask(uint8_t command) {
    switch ((uint8_t)(command & 0x7Fu)) {
    case 1u:
        return BT_APP_BUTTON_RANGE;
    case 2u:
        return BT_APP_BUTTON_HOLD;
    case 3u:
        return BT_APP_BUTTON_REL;
    case 4u:
        return BT_APP_BUTTON_DOWN;
    case 5u:
        return BT_APP_BUTTON_MODE;
    case 6u:
        return BT_APP_BUTTON_MINMAX;
    case 7u:
        return BT_APP_BUTTON_MEM;
    case 8u:
        return BT_APP_BUTTON_SETUP;
    default:
        return 0u;
    }
}

static void queue_echo_frame(const uint8_t *frame, uint8_t len) {
    if (frame == 0 || len == 0u || len > sizeof(g_echo_frame)) {
        return;
    }
    for (uint8_t i = 0; i < len; ++i) {
        g_echo_frame[i] = frame[i];
    }
    g_echo_len = len;
}

static bool service_pending_echo(void) {
    if (g_echo_len == 0u) {
        return true;
    }
    if (!bluetooth_send_bytes_irq(g_echo_frame, g_echo_len)) {
        return false;
    }
    g_echo_len = 0u;
    return true;
}

static bt_app_rx_kind_t feed_stock_app_byte(uint8_t byte, uint8_t *button_mask,
                                            uint8_t datetime[6], uint8_t *completed_len) {
    if (g_rx_len == 0u) {
        if (byte != 0xF4u && byte != 0xF8u) {
            return BT_APP_RX_NONE;
        }
        g_rx_frame[g_rx_len++] = byte;
        return BT_APP_RX_NONE;
    }

    uint8_t expected = g_rx_frame[0] == 0xF8u ? 15u : 5u;
    g_rx_frame[g_rx_len++] = byte;
    if (g_rx_len < expected) {
        return BT_APP_RX_NONE;
    }

    *completed_len = expected;
    g_rx_len = 0u;
    if (g_rx_frame[0] == 0xF4u) {
        uint8_t cmd0 = 0;
        uint8_t cmd1 = 0;
        if (!decode_ascii_hex_at(g_rx_frame, 1u, &cmd0) ||
            !decode_ascii_hex_at(g_rx_frame, 3u, &cmd1) ||
            cmd0 != cmd1) {
            return BT_APP_RX_NONE;
        }
        *button_mask = f4_command_to_button_mask(cmd0);
        return *button_mask != 0u ? BT_APP_RX_F4 : BT_APP_RX_NONE;
    }

    uint8_t xor_sum = 0u;
    for (uint8_t i = 0; i < 6u; ++i) {
        if (!decode_ascii_hex_at(g_rx_frame, (uint8_t)(1u + i * 2u), &datetime[i])) {
            return BT_APP_RX_NONE;
        }
        xor_sum ^= datetime[i];
    }
    uint8_t expected_xor = 0u;
    if (!decode_ascii_hex_at(g_rx_frame, 13u, &expected_xor) || xor_sum != expected_xor) {
        return BT_APP_RX_NONE;
    }
    return BT_APP_RX_F8;
}

void bluetooth_app_task_init(void) {
    g_rx_len = 0u;
    g_event_head = 0u;
    g_event_tail = 0u;
    g_event_count = 0u;
    g_echo_len = 0u;
    for (uint8_t i = 0; i < BT_APP_EVENT_QUEUE_SIZE; ++i) {
        clear_event(&g_event_queue[i]);
    }
}

bool bluetooth_app_task_poll(bool stock_app_mode, bluetooth_app_event_t *event) {
    if (event == 0) {
        return false;
    }
    clear_event(event);

    uint8_t byte = 0;
    while (bluetooth_receive_byte(&byte)) {
        if (!stock_app_mode && uart_update_rx_byte(byte)) {
            event->kind = BLUETOOTH_APP_EVENT_UART_COMMAND;
            event->value = byte;
            return true;
        }

        uint8_t button_mask = 0;
        uint8_t completed_len = 0;
        bt_app_rx_kind_t kind =
            feed_stock_app_byte(byte, &button_mask, event->datetime, &completed_len);
        if (kind == BT_APP_RX_F4) {
            queue_echo_frame(g_rx_frame, completed_len);
            event->kind = BLUETOOTH_APP_EVENT_BUTTON;
            event->value = button_mask;
            return true;
        }
        if (kind == BT_APP_RX_F8) {
            queue_echo_frame(g_rx_frame, completed_len);
            event->kind = BLUETOOTH_APP_EVENT_DATETIME;
            event->value = 0xF8u;
            return true;
        }
    }
    return false;
}

void bluetooth_app_task_service(void) {
    bluetooth_app_event_t event;
    for (uint8_t processed = 0u; processed < BT_APP_EVENT_QUEUE_SIZE; ++processed) {
        if (!service_pending_echo()) {
            return;
        }
        if (!bluetooth_app_task_poll(uart_update_bt_stream_enabled(), &event)) {
            return;
        }
        push_event(&event);
    }
}

bool bluetooth_app_task_take_event(bluetooth_app_event_t *event) {
    if (event == 0) {
        return false;
    }
    if (g_event_count == 0u) {
        clear_event(event);
        return false;
    }
    *event = g_event_queue[g_event_tail];
    clear_event(&g_event_queue[g_event_tail]);
    g_event_tail = (uint8_t)((g_event_tail + 1u) % BT_APP_EVENT_QUEUE_SIZE);
    --g_event_count;
    return true;
}
