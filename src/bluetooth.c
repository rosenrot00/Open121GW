#include "bluetooth.h"

#include "clock.h"
#include "gpio.h"
#include "hw.h"

#define USART2_APB1EN BIT(17)
#define BT_ENABLE_PORT GPIOF_BASE
#define BT_ENABLE_PIN 15u
#define BT_UART_PORT GPIOD_BASE
#define BT_UART_TX_PIN 5u
#define BT_UART_RX_PIN 6u
#define BT_UART_AF 7u
#define BT_USART2_IRQ 38u
#define BT_RX_BUF_SIZE 128u
#define BT_TX_QUEUE_SIZE 2048u

static volatile uint8_t g_bt_rx_buf[BT_RX_BUF_SIZE];
static volatile uint8_t g_bt_rx_head;
static volatile uint8_t g_bt_rx_tail;
static volatile uint8_t g_bt_tx_queue[BT_TX_QUEUE_SIZE];
static volatile uint16_t g_bt_tx_head;
static volatile uint16_t g_bt_tx_tail;

static void bluetooth_rx_push(uint8_t byte) {
    uint8_t next = (uint8_t)((g_bt_rx_head + 1u) & (BT_RX_BUF_SIZE - 1u));
    if (next != g_bt_rx_tail) {
        g_bt_rx_buf[g_bt_rx_head] = byte;
        g_bt_rx_head = next;
    }
}

void bluetooth_set_baud(uint32_t pclk_hz, uint32_t baud) {
    uint32_t usartdiv100 = (25u * pclk_hz) / (4u * baud);
    uint32_t mantissa = usartdiv100 / 100u;
    uint32_t fraction = ((usartdiv100 - mantissa * 100u) * 16u + 50u) / 100u;
    uint32_t brr = (mantissa << 4) | (fraction & 0x0Fu);
    REG32(USART_BRR(USART2_BASE)) = brr;
}

void bluetooth_set_enable(bool enable_ble) {
    gpio_config_pin(BT_ENABLE_PORT, BT_ENABLE_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    if (enable_ble) {
        gpio_set(BT_ENABLE_PORT, BT_ENABLE_PIN);
    } else {
        gpio_clear(BT_ENABLE_PORT, BT_ENABLE_PIN);
    }
}

void bluetooth_init_uart(uint32_t pclk_hz, uint32_t baud, bool enable_ble) {
    bluetooth_set_enable(enable_ble);

    gpio_config_pin(BT_UART_PORT, BT_UART_TX_PIN, GPIO_MODE_AF, GPIO_PULL_UP, BT_UART_AF);
    gpio_config_pin(BT_UART_PORT, BT_UART_RX_PIN, GPIO_MODE_AF, GPIO_PULL_UP, BT_UART_AF);
    REG32(GPIO_OSPEEDR(BT_UART_PORT)) |= (3u << (BT_UART_TX_PIN * 2u)) | (3u << (BT_UART_RX_PIN * 2u));

    REG32(RCC_APB1ENR) |= USART2_APB1EN;
    (void)REG32(RCC_APB1ENR);

    REG32(USART_CR1(USART2_BASE)) = 0;
    REG32(USART_CR2(USART2_BASE)) = 0;
    REG32(USART_CR3(USART2_BASE)) = 0;
    g_bt_rx_head = 0;
    g_bt_rx_tail = 0;
    g_bt_tx_head = 0;
    g_bt_tx_tail = 0;

    bluetooth_set_baud(pclk_hz, baud);
    REG32(USART_CR1(USART2_BASE)) = BIT(13) | BIT(5) | BIT(3) | BIT(2);
    REG32(NVIC_ICPR0 + (BT_USART2_IRQ / 32u) * 4u) = BIT(BT_USART2_IRQ % 32u);
    REG32(NVIC_ISER0 + (BT_USART2_IRQ / 32u) * 4u) = BIT(BT_USART2_IRQ % 32u);
}

void bluetooth_init_19200(uint32_t pclk_hz) {
    bluetooth_init_uart(pclk_hz, 19200u, true);
}

void bluetooth_power_down(void) {
    REG32(NVIC_ICER0 + (BT_USART2_IRQ / 32u) * 4u) = BIT(BT_USART2_IRQ % 32u);
    REG32(NVIC_ICPR0 + (BT_USART2_IRQ / 32u) * 4u) = BIT(BT_USART2_IRQ % 32u);

    REG32(USART_CR1(USART2_BASE)) = 0;
    REG32(USART_CR2(USART2_BASE)) = 0;
    REG32(USART_CR3(USART2_BASE)) = 0;
    REG32(RCC_APB1ENR) &= ~USART2_APB1EN;

    g_bt_rx_head = 0;
    g_bt_rx_tail = 0;
    g_bt_tx_head = 0;
    g_bt_tx_tail = 0;

    gpio_config_pin(BT_ENABLE_PORT, BT_ENABLE_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE, 0);
    gpio_clear(BT_ENABLE_PORT, BT_ENABLE_PIN);
    gpio_config_pin(BT_UART_PORT, BT_UART_TX_PIN, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
    gpio_config_pin(BT_UART_PORT, BT_UART_RX_PIN, GPIO_MODE_ANALOG, GPIO_PULL_NONE, 0);
}

bool bluetooth_send_byte(uint8_t byte) {
    uint32_t timeout = 200000u;
    while (bluetooth_tx_busy()) {
        if (timeout-- == 0u) {
            return false;
        }
    }
    while ((REG32(USART_SR(USART2_BASE)) & BIT(7)) == 0u) {
        if (timeout-- == 0u) {
            return false;
        }
    }
    REG32(USART_DR(USART2_BASE)) = byte;
    return true;
}

bool bluetooth_wait_tx_complete(uint32_t timeout) {
    while (bluetooth_tx_busy()) {
        if (timeout-- == 0u) {
            return false;
        }
    }
    while ((REG32(USART_SR(USART2_BASE)) & BIT(6)) == 0u) {
        if (timeout-- == 0u) {
            return false;
        }
    }
    return true;
}

bool bluetooth_tx_busy(void) {
    return g_bt_tx_head != g_bt_tx_tail;
}

bool bluetooth_send_bytes_irq(const uint8_t *data, uint8_t len) {
    if (data == 0 || len == 0u || bluetooth_tx_busy()) {
        return false;
    }

    uint16_t head = g_bt_tx_head;
    for (uint8_t i = 0; i < len; ++i) {
        uint16_t next = (uint16_t)((head + 1u) & (BT_TX_QUEUE_SIZE - 1u));
        if (next == g_bt_tx_tail) {
            return false;
        }
        g_bt_tx_queue[head] = data[i];
        head = next;
    }
    g_bt_tx_head = head;
    REG32(USART_CR1(USART2_BASE)) |= BIT(7);
    return true;
}

bool bluetooth_queue_byte_irq(uint8_t byte) {
    uint16_t head = g_bt_tx_head;
    uint16_t next = (uint16_t)((head + 1u) & (BT_TX_QUEUE_SIZE - 1u));
    if (next == g_bt_tx_tail) {
        return false;
    }
    g_bt_tx_queue[head] = byte;
    g_bt_tx_head = next;
    REG32(USART_CR1(USART2_BASE)) |= BIT(7);
    return true;
}

void bluetooth_send_frame(const uint8_t payload17[17]) {
    uint8_t frame[19];
    uint8_t xor_sum = 0;

    frame[0] = 0xF2u;
    for (uint8_t i = 0; i < 17u; ++i) {
        frame[i + 1u] = payload17[i];
    }
    for (uint8_t i = 0; i < 18u; ++i) {
        xor_sum ^= frame[i];
    }
    frame[18] = xor_sum;

    (void)bluetooth_send_bytes_irq(frame, sizeof(frame));
}

void bluetooth_send_heartbeat(uint32_t ticks, uint32_t cal_hash, bool cal_present) {
    uint8_t payload[17] = {0};

    payload[0] = 0x45u; /* 'E' */
    payload[1] = 0x45u; /* 'E' */
    payload[2] = 0x56u; /* 'V' */
    payload[3] = cal_present ? 1u : 0u;
    payload[4] = (uint8_t)ticks;
    payload[5] = (uint8_t)(ticks >> 8);
    payload[6] = (uint8_t)(ticks >> 16);
    payload[7] = (uint8_t)(ticks >> 24);
    payload[8] = (uint8_t)cal_hash;
    payload[9] = (uint8_t)(cal_hash >> 8);
    payload[10] = (uint8_t)(cal_hash >> 16);
    payload[11] = (uint8_t)(cal_hash >> 24);

    bluetooth_send_frame(payload);
}

void bluetooth_make_meter_frame(uint8_t out[19], uint32_t meter_id, uint8_t mode,
                                uint8_t range, bool overrange, int32_t main_value,
                                int16_t sub_value, bool sub_fahrenheit,
                                uint8_t unit_scale, uint8_t bargraph,
                                uint8_t icon0, uint8_t icon1, uint8_t icon2) {
    uint32_t id = meter_id % 100000u;
    uint8_t id0 = (uint8_t)(id % 10u);
    id /= 10u;
    uint8_t id1 = (uint8_t)(id % 10u);
    id /= 10u;
    uint8_t id2 = (uint8_t)(id % 10u);
    id /= 10u;
    uint8_t id3 = (uint8_t)(id % 10u);
    id /= 10u;
    uint8_t id4 = (uint8_t)(id % 10u);
    bool negative = main_value < 0;
    uint32_t magnitude = negative ? (uint32_t)(-(main_value + 1)) + 1u : (uint32_t)main_value;
    bool sub_negative = sub_value < 0;
    uint16_t sub_magnitude = sub_negative ? (uint16_t)(-(sub_value + 1)) + 1u : (uint16_t)sub_value;
    uint8_t mode_flags = (uint8_t)(mode & 0x1Fu);
    uint8_t flags_range = (uint8_t)(range & 0x0Fu);
    uint8_t sub_flags = 1u;
    if ((magnitude & 0x20000u) != 0u) {
        mode_flags |= 0x80u;
    }
    if ((magnitude & 0x10000u) != 0u) {
        mode_flags |= 0x40u;
    }
    if (overrange) {
        flags_range |= 0x80u;
    }
    if (negative) {
        flags_range |= 0x40u;
    }
    if (sub_negative) {
        sub_flags |= 0x40u;
    }

    out[0] = 0xF2u;
    out[1] = 0u;
    out[2] = (uint8_t)(id4 & 0x0Fu);
    out[3] = (uint8_t)(((id3 & 0x0Fu) << 4) | (id2 & 0x0Fu));
    out[4] = (uint8_t)(((id1 & 0x0Fu) << 4) | (id0 & 0x0Fu));
    out[5] = mode_flags;
    out[6] = flags_range;
    out[7] = (uint8_t)(magnitude >> 8);
    out[8] = (uint8_t)magnitude;
    out[9] = sub_fahrenheit ? 0x64u : 0x69u;
    out[10] = sub_flags;
    out[11] = (uint8_t)(sub_magnitude >> 8);
    out[12] = (uint8_t)sub_magnitude;
    out[13] = unit_scale;
    out[14] = (uint8_t)bargraph;
    out[15] = icon0;
    out[16] = icon1;
    out[17] = icon2;

    uint8_t xor_sum = 0u;
    for (uint8_t i = 0; i < 18u; ++i) {
        xor_sum ^= out[i];
    }
    out[18] = xor_sum;
}

bool bluetooth_receive_byte(uint8_t *byte) {
    uint8_t tail = g_bt_rx_tail;
    if (tail == g_bt_rx_head) {
        return false;
    }
    *byte = g_bt_rx_buf[tail];
    g_bt_rx_tail = (uint8_t)((tail + 1u) & (BT_RX_BUF_SIZE - 1u));
    return true;
}

void bluetooth_poll_rx(void) {
    uint8_t byte;
    while (bluetooth_receive_byte(&byte)) {
    }
}

void bluetooth_irq_handler(void) {
    uint32_t sr = REG32(USART_SR(USART2_BASE));
    if ((sr & BIT(7)) != 0u && (REG32(USART_CR1(USART2_BASE)) & BIT(7)) != 0u) {
        uint16_t tail = g_bt_tx_tail;
        if (tail != g_bt_tx_head) {
            REG32(USART_DR(USART2_BASE)) = g_bt_tx_queue[tail];
            g_bt_tx_tail = (uint16_t)((tail + 1u) & (BT_TX_QUEUE_SIZE - 1u));
        } else {
            REG32(USART_CR1(USART2_BASE)) &= ~BIT(7);
        }
    }
    while ((sr & (BIT(5) | BIT(3))) != 0u) {
        uint8_t byte = (uint8_t)REG32(USART_DR(USART2_BASE));
        if ((sr & BIT(5)) != 0u) {
            bluetooth_rx_push(byte);
        }
        sr = REG32(USART_SR(USART2_BASE));
    }
}
