#include <stdbool.h>
#include <stdint.h>

#define BIT(n) (1u << (n))
#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define REG16(addr) (*(volatile uint16_t *)(uintptr_t)(addr))
#define REG8(addr)  (*(volatile uint8_t *)(uintptr_t)(addr))

#define RCC_BASE 0x40023800u
#define RCC_CR (RCC_BASE + 0x00u)
#define RCC_ICSCR (RCC_BASE + 0x04u)
#define RCC_CFGR (RCC_BASE + 0x08u)
#define RCC_CIR (RCC_BASE + 0x0Cu)
#define RCC_AHBENR (RCC_BASE + 0x1Cu)
#define RCC_APB2ENR (RCC_BASE + 0x20u)
#define RCC_APB1ENR (RCC_BASE + 0x24u)

#define GPIOA_BASE 0x40020000u
#define GPIOC_BASE 0x40020800u
#define GPIOD_BASE 0x40020C00u
#define GPIOE_BASE 0x40021000u
#define GPIOF_BASE 0x40021400u
#define GPIOG_BASE 0x40021800u
#define GPIO_MODER(base) ((base) + 0x00u)
#define GPIO_OTYPER(base) ((base) + 0x04u)
#define GPIO_OSPEEDR(base) ((base) + 0x08u)
#define GPIO_PUPDR(base) ((base) + 0x0Cu)
#define GPIO_IDR(base)   ((base) + 0x10u)
#define GPIO_ODR(base) ((base) + 0x14u)
#define GPIO_BSRR(base) ((base) + 0x18u)
#define GPIO_AFRL(base) ((base) + 0x20u)

#define APP_FLASH_BASE 0x08006000u
#define UPD_APP_MAX_SIZE 0x0005A000u
#define UPD_FLASH_PAGE_SIZE 256u
#define UPD_FLASH_HALF_PAGE_SIZE 128u
#define UPD_STREAM_CHUNK_MAX 128u
#define UPD_LINE_MAX 280u
#define DIAG_LINE_MAX 96u
#define UPD_RAM_MIN 0x20000000u
#define UPD_RAM_MAX 0x20014000u

#define USART2_BASE 0x40004400u
#define USART_SR(base)  ((base) + 0x00u)
#define USART_DR(base)  ((base) + 0x04u)
#define USART_BRR(base) ((base) + 0x08u)
#define USART_CR1(base) ((base) + 0x0Cu)
#define USART_CR2(base) ((base) + 0x10u)
#define USART_CR3(base) ((base) + 0x14u)
#define USART2_BRR_STOCK_19200 0x00DAu
#define USART2_BRR_FLASH_38400 0x006Du
#define USART2_BRR_FLASH_57600 0x0049u
#define USART2_BRR_FLASH_115200 0x0024u
#define USART2_BRR_FLASH_230400 0x0012u
#define USART2_BRR_FLASH_460800 0x0009u
#define USART2_BRR_FLASH_921600 0x0005u

#define LCD_BASE 0x40002400u
#define LCD_RAM_BASE (LCD_BASE + 0x14u)

#define FLASH_BASE 0x40023C00u
#define FLASH_PECR (FLASH_BASE + 0x04u)
#define FLASH_PEKEYR (FLASH_BASE + 0x0Cu)
#define FLASH_PRGKEYR (FLASH_BASE + 0x10u)
#define FLASH_SR (FLASH_BASE + 0x18u)
#define FLASH_PECR_PELOCK BIT(0)
#define FLASH_PECR_PRGLOCK BIT(1)
#define FLASH_PECR_PROG BIT(3)
#define FLASH_PECR_ERASE BIT(9)
#define FLASH_PECR_FPRG BIT(10)
#define FLASH_SR_BSY BIT(0)
#define FLASH_SR_EOP BIT(1)
#define FLASH_SR_WRPERR BIT(8)
#define FLASH_SR_PGAERR BIT(9)
#define FLASH_SR_SIZERR BIT(10)
#define FLASH_SR_OPTVERR BIT(11)
#define FLASH_SR_ERRORS (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_OPTVERR)

#define SCB_AIRCR 0xE000ED0Cu
#define SCB_ACTLR 0xE000E008u
#define SYST_CSR 0xE000E010u
#define NVIC_ISER0 0xE000E100u
#define NVIC_ICER0 0xE000E180u
#define NVIC_ICPR0 0xE000E280u

#define G_DMM_MODE 0x2000803Cu
#define G_DMM_RANGE 0x2000803Du
#define G_MANUAL_RANGE_OR_HOLD_FLAG 0x20008042u
#define G_DISPLAY_OVERRANGE_FLAG 0x20008046u
#define G_MODE_CHANGE_REQUIRES_MEASUREMENT_RESET 0x2000804Au
#define G_RANGE_CHANGED_FLAG 0x2000804Bu
#define G_MEASUREMENT_READY_FOR_DISPLAY 0x2000805Du
#define G_FRONTEND_STATUS_MASK 0x2000807Fu
#define G_FRONTEND_RX_REGS 0x200077D0u
#define G_FRONTEND_EVENT_STATUS_1E 0x200077EEu
#define G_FRONTEND_EVENT_MASK_1F 0x200077EFu
#define G_FRONTEND_RX_REG20 0x200077F0u
#define G_FRONTEND_RX_REG29 0x200077F9u
#define G_CAP_WRAP_COUNTER 0x20007E3Cu
#define G_CAP_REF_COUNTER 0x20007E40u
#define G_CAP_COUNT_COUNTER 0x20007E44u
#define G_CAP_ELAPSED_TICKS 0x20007E48u
#define G_CAP_RAW_DISPLAY_VALUE 0x20007E68u
#define G_MEASURE_VALUE 0x20007D64u
#define G_DISPLAY_VALUE 0x20007D68u
#define G_RAW_SAMPLE_LAST 0x20007D74u
#define G_AVG_SAMPLE_VALUE 0x20007DD4u
#define G_TEMP_COLD_JUNCTION_EMF 0x20007E7Cu
#define G_TEMP_CONVERTED_CENTIDEG 0x20007FCCu
#define G_TEMP_ADC_RING 0x20007BA8u
#define G_TEMP_ADC_LATEST_RAW 0x20007FCEu
#define G_TEMP_ADC_AVG_RAW 0x20007FD0u
#define G_AUX_ADC_LATEST_RAW 0x20007FD2u
#define G_TEMP_ADC_FILTER_COUNT 0x20007FD4u
#define G_TEMP_CAL_OFFSET_CENTIDEG 0x20007FBEu
#define G_TEMP_ADC_OUT_OF_RANGE 0x200080AFu
#define G_BATTERY_PERCENT 0x20007FBAu
#define G_BATTERY_LOW_LATCHED 0x2000803Bu
#define G_BATTERY_LOW_CANDIDATE 0x200080F9u
#define G_BATTERY_LOW_DEBOUNCE_COUNT 0x20007FDCu
#define G_CAL_ZERO_OFFSET_RAW 0x20007E00u
#define G_CAL_GAIN_FACTOR_FP 0x20007E10u
#define G_DUAL_BANK0_VALUE 0x20007D7Cu
#define G_DUAL_BANK1_VALUE 0x20007D80u
#define G_DUAL_BANK_ACTIVE 0x200080C4u
#define G_ALT_RANGE_INDEX_C6 0x200080C6u
#define G_ALT_RANGE_INDEX_C7 0x200080C7u
#define G_FRONTEND_TIMING_WORD 0x2000001Cu
#define G_MODE0_ALT_OR_REL_FLAG 0x20008098u
#define G_FRONTEND_TIMING_TOGGLE_PHASE 0x2000809Bu
#define G_FRONTEND_TIMING_TOGGLE_INHIBIT 0x200080A5u
#define G_TEMPERATURE_UNIT_C_FLAG 0x200080ABu
#define G_MEASURE_ALT_PATH_B7 0x200080B7u
#define G_MEASURE_ALT_PATH_B9 0x200080B9u
#define G_VOLTAGE_ALT_BANK_ACTIVE 0x200080BAu
#define G_VOLTAGE_ALT_SUBPATH_BD 0x200080BDu
#define G_CAP_TIMEOUT_TICKS 0x20007FC4u
#define G_CAP_TIMEOUT_COUNTER 0x20007FC6u
#define G_CAP_TIMEOUT_PENDING 0x200080A3u
#define G_CAP_MANUAL_RANGE4_HOLDOFF 0x200080A4u
#define G_CAP_FORCE_LOW_RANGE_AFTER_TIMEOUT 0x200080A2u
#define G_CAP_OPTION_TABLE_ENABLED 0x20007FDFu
#define G_BT_TX_FRAME_F2 0x20007AE0u
#define G_BT_STREAMING_ENABLED 0x2000802Bu
#define G_BT_RX_FRAME_READY 0x20008030u
#define G_FREQ_REFERENCE_CAL_HZ 0x20007E18u
#define G_ROTARY_STABLE_POSITION 0x20008065u
#define G_ROTARY_CHANGE_PENDING 0x20008064u
#define G_ROTARY_CANDIDATE_POSITION 0x20008066u
#define G_ROTARY_DEBOUNCE_LOCKOUT 0x200080EBu
#define G_ROTARY_DEBOUNCE_COUNT 0x200080ECu
#define G_ROTARY_LAST_SAMPLE 0x2000023Cu

#define DIAG_RAM_BASE 0x20001E00u
#define DIAG_MAGIC_ADDR (DIAG_RAM_BASE + 0x00u)
#define DIAG_RX_LEN_ADDR (DIAG_RAM_BASE + 0x04u)
#define DIAG_DEBUG_ENABLED_ADDR (DIAG_RAM_BASE + 0x05u)
#define DIAG_HELLO_ENABLED_ADDR (DIAG_RAM_BASE + 0x06u)
#define DIAG_HELLO_DIV_ADDR (DIAG_RAM_BASE + 0x07u)
#define DIAG_TX_DIV_ADDR (DIAG_RAM_BASE + 0x08u)
#define DIAG_UPD_PENDING_ADDR (DIAG_RAM_BASE + 0x09u)
#define DIAG_UPD_FLASH_MODE_ADDR (DIAG_RAM_BASE + 0x0Au)
#define DIAG_RX_PENDING_ADDR (DIAG_RAM_BASE + 0x0Bu)
#define DIAG_UPD_SIZE_ADDR (DIAG_RAM_BASE + 0x0Cu)
#define DIAG_UPD_CRC_ADDR (DIAG_RAM_BASE + 0x10u)
#define DIAG_UPD_DATA_BAUD_ADDR (DIAG_RAM_BASE + 0x14u)
#define DIAG_ROTARY_OVERRIDE_ENABLED_ADDR (DIAG_RAM_BASE + 0x18u)
#define DIAG_ROTARY_OVERRIDE_VALUE_ADDR (DIAG_RAM_BASE + 0x19u)
#define DIAG_TEMP_OVERRIDE_ENABLED_ADDR (DIAG_RAM_BASE + 0x1Au)
#define DIAG_TEMP_OVERRIDE_TENTHS_ADDR (DIAG_RAM_BASE + 0x1Cu)
#define DIAG_P10_OVERRIDE_ENABLED_ADDR (DIAG_RAM_BASE + 0x1Eu)
#define DIAG_P10_OVERRIDE_MODE_ADDR (DIAG_RAM_BASE + 0x1Fu)
#define DIAG_P10_OVERRIDE_RANGE_ADDR (DIAG_RAM_BASE + 0x20u)
#define DIAG_BAT_OVERRIDE_ENABLED_ADDR (DIAG_RAM_BASE + 0x21u)
#define DIAG_BAT_OVERRIDE_PERCENT_ADDR (DIAG_RAM_BASE + 0x22u)
#define DIAG_BAT_OVERRIDE_LOW_ADDR (DIAG_RAM_BASE + 0x23u)
#define DIAG_DIRECT_MODE_PENDING_ADDR (DIAG_RAM_BASE + 0x24u)
#define DIAG_DIRECT_ACK_PENDING_ADDR (DIAG_RAM_BASE + 0x25u)
#define DIAG_RX_BUF_ADDR (DIAG_RAM_BASE + 0x28u)
#define DIAG_MAGIC 0x4455424Au

#define STOCK_CAP_COMPUTE ((void (*)(void))(uintptr_t)0x0800B33Du)
#define STOCK_CAP_TIMEOUT ((void (*)(void))(uintptr_t)0x0800B541u)
#define STOCK_DMM_MEASUREMENT_DISPATCH ((void (*)(void))(uintptr_t)0x080061E5u)
#define STOCK_DMM_RESET_MEASUREMENT_STATE ((void (*)(uint8_t))(uintptr_t)0x0800D2D1u)
#define STOCK_USART_READ_DATA ((uint16_t (*)(uint32_t))(uintptr_t)0x0801CC01u)
#define STOCK_BT_BUILD_FRAME ((void (*)(void))(uintptr_t)0x0800C5D3u)
#define STOCK_BT_PROCESS_RX_COMMAND_FRAME ((void (*)(void))(uintptr_t)0x0800C333u)
#define STOCK_BT_UART_INIT_19200 ((void (*)(void))(uintptr_t)0x0801C391u)
#define STOCK_BT_UART_DISABLE ((void (*)(void))(uintptr_t)0x0801C955u)
#define STOCK_BOARD_LOW_LEVEL_INIT ((void (*)(void))(uintptr_t)0x0801BE77u)
#define STOCK_SYSTEM_INIT ((void (*)(void))(uintptr_t)0x08025F71u)
#define STOCK_RUNTIME_ENTRY ((void (*)(void))(uintptr_t)0x0802919Du)
#define STOCK_ROTARY_APPLY_POSITION ((void (*)(void))(uintptr_t)0x080143EFu)
#define STOCK_ROTARY_SAMPLE_GPIOH_ACTIVE_LOW ((uint8_t (*)(void))(uintptr_t)0x08014A15u)
#define STOCK_MODE_CYCLE_SECONDARY ((void (*)(void))(uintptr_t)0x08014501u)
#define STOCK_UI_HANDLE_KEY_EVENT ((void (*)(uint8_t))(uintptr_t)0x08014C3Du)
#define STOCK_FRONTEND_CONFIGURE_FOR_MODE_RANGE ((void (*)(uint8_t, uint8_t))(uintptr_t)0x0800B975u)
#define STOCK_FRONTEND_APPLY_MUX_ROUTING ((void (*)(uint8_t, uint8_t))(uintptr_t)0x0800FBE9u)
#define STOCK_FRONTEND_READ_REGISTERS ((void (*)(uint32_t, uint8_t, uint16_t))(uintptr_t)0x0800C0C1u)

#define RAMFUNC __attribute__((section(".ramfunc"), noinline))

extern uint8_t __ramfunc_load;
extern uint8_t __ramfunc_start;
extern uint8_t __ramfunc_end;

static void diag_state_init(void);
static void diag_rx_byte(uint8_t byte);
static void enter_ram_stream(bool flash_mode, uint32_t size, uint32_t crc, uint32_t data_baud);
uint8_t hook_rotary_sample_override(void);
static void diag_apply_temperature_override(void);
static void diag_apply_battery_override(void);
static void diag_apply_pending_direct_mode(void);

static void uart_keep_stock_baud(void) {
    uint32_t cr1 = REG32(USART_CR1(USART2_BASE));
    while ((REG32(USART_SR(USART2_BASE)) & BIT(6)) == 0u) {
    }
    REG32(USART_CR1(USART2_BASE)) = cr1 & ~BIT(13);
    REG32(USART_BRR(USART2_BASE)) = USART2_BRR_FLASH_115200;
    REG32(USART_CR1(USART2_BASE)) = cr1;
}

static bool uart_send_char(char ch) {
    uint32_t cr1 = REG32(USART_CR1(USART2_BASE));
    if ((cr1 & (BIT(13) | BIT(3))) != (BIT(13) | BIT(3))) {
        return false;
    }

    uint32_t guard = 0x4000u;
    while ((REG32(USART_SR(USART2_BASE)) & BIT(7)) == 0u) {
        if (--guard == 0u) {
            return false;
        }
    }
    REG32(USART_DR(USART2_BASE)) = (uint8_t)ch;
    return true;
}

static bool uart_send_text(const char *text) {
    while (*text != '\0') {
        if (!uart_send_char(*text++)) {
            return false;
        }
    }
    return true;
}

static char hex_digit(uint8_t value) {
    value &= 0x0Fu;
    if (value < 10u) {
        return (char)((uint8_t)'0' + value);
    }
    return (char)((uint8_t)'A' + value - 10u);
}

static bool uart_send_hex8(uint8_t value) {
    return uart_send_char(hex_digit((uint8_t)(value >> 4))) && uart_send_char(hex_digit(value));
}

static bool uart_send_hex16(uint16_t value) {
    return uart_send_hex8((uint8_t)(value >> 8)) && uart_send_hex8((uint8_t)value);
}

static bool uart_send_hex32(uint32_t value) {
    return uart_send_hex8((uint8_t)(value >> 24)) && uart_send_hex8((uint8_t)(value >> 16)) &&
           uart_send_hex8((uint8_t)(value >> 8)) && uart_send_hex8((uint8_t)value);
}

static bool direct_uart_ready(void) {
    return (REG32(USART_BRR(USART2_BASE)) == USART2_BRR_FLASH_115200) &&
           ((REG32(USART_CR1(USART2_BASE)) & (BIT(13) | BIT(5) | BIT(3) | BIT(2))) ==
            (BIT(13) | BIT(5) | BIT(3) | BIT(2)));
}

static void bt_ctl_pf15_set(bool enabled) {
    REG32(RCC_AHBENR) |= BIT(5);
    (void)REG32(RCC_AHBENR);

    REG32(GPIO_MODER(GPIOF_BASE)) = (REG32(GPIO_MODER(GPIOF_BASE)) & ~(3u << 30)) | (1u << 30);
    REG32(GPIO_OTYPER(GPIOF_BASE)) &= ~BIT(15);
    REG32(GPIO_OSPEEDR(GPIOF_BASE)) |= (3u << 30);
    REG32(GPIO_PUPDR(GPIOF_BASE)) &= ~(3u << 30);
    REG32(GPIO_BSRR(GPIOF_BASE)) = enabled ? BIT(15) : BIT(31);
}

static void direct_uart_init_ble_off(void) {
    if (direct_uart_ready()) {
        bt_ctl_pf15_set(false);
        return;
    }

    REG32(RCC_AHBENR) |= BIT(3) | BIT(5);
    (void)REG32(RCC_AHBENR);

    bt_ctl_pf15_set(false);

    REG32(GPIO_MODER(GPIOD_BASE)) =
        (REG32(GPIO_MODER(GPIOD_BASE)) & ~((3u << 10) | (3u << 12))) | (2u << 10) | (2u << 12);
    REG32(GPIO_OTYPER(GPIOD_BASE)) &= ~(BIT(5) | BIT(6));
    REG32(GPIO_OSPEEDR(GPIOD_BASE)) |= (3u << 10) | (3u << 12);
    REG32(GPIO_PUPDR(GPIOD_BASE)) =
        (REG32(GPIO_PUPDR(GPIOD_BASE)) & ~((3u << 10) | (3u << 12))) | (1u << 10) | (1u << 12);
    REG32(GPIO_AFRL(GPIOD_BASE)) =
        (REG32(GPIO_AFRL(GPIOD_BASE)) & ~((0xFu << 20) | (0xFu << 24))) | (7u << 20) | (7u << 24);

    REG32(RCC_APB1ENR) |= BIT(17);
    (void)REG32(RCC_APB1ENR);

    REG32(USART_CR1(USART2_BASE)) = 0;
    REG32(USART_CR2(USART2_BASE)) = 0;
    REG32(USART_CR3(USART2_BASE)) = 0;
    REG32(USART_BRR(USART2_BASE)) = USART2_BRR_FLASH_115200;
    REG32(USART_CR1(USART2_BASE)) = BIT(13) | BIT(5) | BIT(3) | BIT(2);
    REG32(NVIC_ISER0 + (38u / 32u) * 4u) = BIT(38u % 32u);
}

static void diag_send_hello_now(void) {
    diag_state_init();
    if (REG8(DIAG_HELLO_ENABLED_ADDR) != 0u) {
        (void)uart_send_text("HALLO stockuart 115200 ble=0\r\n");
    }
}

static void diag_poll_uart_rx(void) {
    uint16_t guard = 96u;
    while ((REG32(USART_SR(USART2_BASE)) & BIT(5)) != 0u && guard-- != 0u) {
        diag_rx_byte((uint8_t)REG32(USART_DR(USART2_BASE)));
    }
}

static bool diag_enter_updater_if_pending(void) {
    diag_state_init();
    if (REG8(DIAG_UPD_PENDING_ADDR) == 0u) {
        return false;
    }
    bool flash_mode = REG8(DIAG_UPD_FLASH_MODE_ADDR) != 0u;
    uint32_t size = REG32(DIAG_UPD_SIZE_ADDR);
    uint32_t crc = REG32(DIAG_UPD_CRC_ADDR);
    uint32_t data_baud = REG32(DIAG_UPD_DATA_BAUD_ADDR);
    REG8(DIAG_UPD_PENDING_ADDR) = 0;
    REG8(G_BT_RX_FRAME_READY) = 0;
    REG8(G_BT_STREAMING_ENABLED) = 0;
    (void)uart_send_text("UPDSTART\r\n");
    enter_ram_stream(flash_mode, size, crc, data_baud);
    return true;
}

void hook_reset_direct_uart(void) {
    STOCK_SYSTEM_INIT();
    direct_uart_init_ble_off();
    STOCK_RUNTIME_ENTRY();
    while (1) {
    }
}

void hook_reset_direct_uart_pf15_high(void) {
    STOCK_SYSTEM_INIT();
    direct_uart_init_ble_off();
    bt_ctl_pf15_set(true);
    STOCK_RUNTIME_ENTRY();
    while (1) {
    }
}

void hook_bt_disable_direct_uart(void) {
    direct_uart_init_ble_off();
}

void hook_bt_init_stock_ble_low(void) {
    STOCK_BT_UART_INIT_19200();
    uart_keep_stock_baud();
    bt_ctl_pf15_set(false);
    REG8(G_BT_STREAMING_ENABLED) = 0;
}

void hook_bt_disable_keep_uart_ble_low(void) {
    direct_uart_init_ble_off();
    REG8(G_BT_STREAMING_ENABLED) = 0;
}

void hook_bt_init_stock_pf15_high(void) {
    STOCK_BT_UART_INIT_19200();
    bt_ctl_pf15_set(true);
}

void hook_bt_disable_stock_pf15_low(void) {
    STOCK_BT_UART_DISABLE();
    bt_ctl_pf15_set(false);
}

void hook_bt_disable_stock_pf15_keep_high(void) {
    STOCK_BT_UART_DISABLE();
    bt_ctl_pf15_set(true);
}

void hook_board_init_uart_ready_ble_low(void) {
    STOCK_BOARD_LOW_LEVEL_INIT();
    STOCK_BT_UART_INIT_19200();
    uart_keep_stock_baud();
    bt_ctl_pf15_set(false);
    REG8(G_BT_STREAMING_ENABLED) = 0;
}

static uint8_t tx_frame_checksum(void) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < 18u; ++i) {
        sum ^= REG8(G_BT_TX_FRAME_F2 + i);
    }
    return sum;
}

static void tx_frame_put_hex8(uint8_t offset, uint8_t value) {
    REG8(G_BT_TX_FRAME_F2 + offset) = (uint8_t)hex_digit((uint8_t)(value >> 4));
    REG8(G_BT_TX_FRAME_F2 + offset + 1u) = (uint8_t)hex_digit(value);
}

void hook_bt_build_frame_safe_diag(void) {
    uart_keep_stock_baud();
    bt_ctl_pf15_set(false);
    REG8(G_BT_STREAMING_ENABLED) = 0;
    diag_poll_uart_rx();
    diag_apply_pending_direct_mode();
    if (diag_enter_updater_if_pending()) {
        return;
    }

    STOCK_BT_BUILD_FRAME();

    diag_state_init();
    uint8_t div = (uint8_t)(REG8(DIAG_TX_DIV_ADDR) + 1u);
    REG8(DIAG_TX_DIV_ADDR) = div;
    if ((div & 0x0Fu) != 0u) {
        return;
    }

    REG8(G_BT_TX_FRAME_F2 + 0u) = 0xF2u;
    REG8(G_BT_TX_FRAME_F2 + 1u) = 'D';
    REG8(G_BT_TX_FRAME_F2 + 2u) = 'B';
    REG8(G_BT_TX_FRAME_F2 + 3u) = 'G';
    REG8(G_BT_TX_FRAME_F2 + 4u) = '0';
    tx_frame_put_hex8(5u, REG8(G_DMM_MODE));
    tx_frame_put_hex8(7u, REG8(G_DMM_RANGE));
    tx_frame_put_hex8(9u, REG8(G_FRONTEND_STATUS_MASK));
    tx_frame_put_hex8(11u, REG8(G_FRONTEND_EVENT_STATUS_1E));
    tx_frame_put_hex8(13u, REG8(G_FRONTEND_EVENT_MASK_1F));
    REG8(G_BT_TX_FRAME_F2 + 15u) = hex_digit(REG8(G_MEASUREMENT_READY_FOR_DISPLAY));
    REG8(G_BT_TX_FRAME_F2 + 16u) = hex_digit(REG8(G_RANGE_CHANGED_FLAG));
    REG8(G_BT_TX_FRAME_F2 + 17u) = hex_digit(div);
    REG8(G_BT_TX_FRAME_F2 + 18u) = tx_frame_checksum();
}

static void diag_send_hello_if_due(void) {
    diag_state_init();
    if (REG8(DIAG_HELLO_ENABLED_ADDR) == 0u) {
        return;
    }
    uint8_t div = (uint8_t)(REG8(DIAG_HELLO_DIV_ADDR) + 1u);
    REG8(DIAG_HELLO_DIV_ADDR) = div;
    if ((div & 0x3Fu) != 0u) {
        return;
    }
    diag_send_hello_now();
}

void hook_cap_timeout_uart_only(void) {
    diag_apply_pending_direct_mode();
    STOCK_CAP_TIMEOUT();
    direct_uart_init_ble_off();
    diag_send_hello_if_due();
}

void hook_measurement_dispatch_uart_hello(void) {
    diag_apply_pending_direct_mode();
    STOCK_DMM_MEASUREMENT_DISPATCH();
    direct_uart_init_ble_off();
    uart_keep_stock_baud();
    REG8(G_BT_STREAMING_ENABLED) = 0;
    diag_poll_uart_rx();
    if (diag_enter_updater_if_pending()) {
        return;
    }
    diag_send_hello_if_due();
}

static void diag_state_init(void) {
    if (REG32(DIAG_MAGIC_ADDR) != DIAG_MAGIC) {
        REG32(DIAG_MAGIC_ADDR) = DIAG_MAGIC;
        REG8(DIAG_RX_LEN_ADDR) = 0;
        REG8(DIAG_DEBUG_ENABLED_ADDR) = 0;
        REG8(DIAG_HELLO_ENABLED_ADDR) = 1;
        REG8(DIAG_HELLO_DIV_ADDR) = 0;
        REG8(DIAG_TX_DIV_ADDR) = 0;
        REG8(DIAG_UPD_PENDING_ADDR) = 0;
        REG8(DIAG_UPD_FLASH_MODE_ADDR) = 0;
        REG8(DIAG_RX_PENDING_ADDR) = 0;
        REG8(DIAG_DIRECT_MODE_PENDING_ADDR) = 0;
        REG32(DIAG_UPD_SIZE_ADDR) = 0;
        REG32(DIAG_UPD_CRC_ADDR) = 0;
        REG32(DIAG_UPD_DATA_BAUD_ADDR) = 115200u;
        REG8(DIAG_ROTARY_OVERRIDE_ENABLED_ADDR) = 0;
        REG8(DIAG_ROTARY_OVERRIDE_VALUE_ADDR) = 0;
        REG8(DIAG_TEMP_OVERRIDE_ENABLED_ADDR) = 0;
        REG16(DIAG_TEMP_OVERRIDE_TENTHS_ADDR) = 250u;
        REG8(DIAG_BAT_OVERRIDE_ENABLED_ADDR) = 0;
        REG8(DIAG_BAT_OVERRIDE_PERCENT_ADDR) = 88u;
        REG8(DIAG_BAT_OVERRIDE_LOW_ADDR) = 0;
        REG8(DIAG_DIRECT_ACK_PENDING_ADDR) = 0;
    }
    if (REG8(DIAG_MAGIC_ADDR) == 0xA5u) {
        (void)hook_rotary_sample_override();
    }
    diag_apply_temperature_override();
    diag_apply_battery_override();
}

static uint8_t diag_debug_enabled(void) {
    diag_state_init();
    return REG8(DIAG_DEBUG_ENABLED_ADDR);
}

static void diag_apply_temperature_override(void) {
    if (REG8(DIAG_TEMP_OVERRIDE_ENABLED_ADDR) == 0u) {
        return;
    }
    int16_t tenths_c = (int16_t)REG16(DIAG_TEMP_OVERRIDE_TENTHS_ADDR);
    REG32(G_TEMP_CONVERTED_CENTIDEG) = (uint32_t)(int32_t)tenths_c * 10u;
}

static void diag_apply_battery_override(void) {
    if (REG8(DIAG_BAT_OVERRIDE_ENABLED_ADDR) == 0u) {
        return;
    }
    uint8_t percent = REG8(DIAG_BAT_OVERRIDE_PERCENT_ADDR);
    if (percent > 99u) {
        percent = 99u;
    }
    uint8_t low = REG8(DIAG_BAT_OVERRIDE_LOW_ADDR) ? 1u : 0u;
    REG8(G_BATTERY_PERCENT) = percent;
    REG8(G_BATTERY_LOW_CANDIDATE) = low;
    REG8(G_BATTERY_LOW_LATCHED) = low;
    REG8(G_BATTERY_LOW_DEBOUNCE_COUNT) = 0;
}

static uint32_t rd24(uint32_t addr) {
    return REG8(addr) | ((uint32_t)REG8(addr + 1u) << 8) | ((uint32_t)REG8(addr + 2u) << 16);
}

static void diag_emit_cap(char tag) {
    if (!diag_debug_enabled() || REG8(G_DMM_MODE) != 0x0Cu) {
        return;
    }

    if (!uart_send_text("SCAP ") ||
        !uart_send_char(tag) ||
        !uart_send_text(" m=") ||
        !uart_send_hex8(REG8(G_DMM_MODE)) ||
        !uart_send_text(" r=") ||
        !uart_send_hex8(REG8(G_DMM_RANGE)) ||
        !uart_send_text(" st=") ||
        !uart_send_hex8(REG8(G_FRONTEND_STATUS_MASK)) ||
        !uart_send_text(" r1e=") ||
        !uart_send_hex8(REG8(G_FRONTEND_EVENT_STATUS_1E)) ||
        !uart_send_text(" r1f=") ||
        !uart_send_hex8(REG8(G_FRONTEND_EVENT_MASK_1F)) ||
        !uart_send_text(" r20=") ||
        !uart_send_hex8(REG8(G_FRONTEND_RX_REG20)) ||
        !uart_send_text(" r29=") ||
        !uart_send_hex8(REG8(G_FRONTEND_RX_REG29)) ||
        !uart_send_text(" cnt=") ||
        !uart_send_hex32(rd24(G_CAP_COUNT_COUNTER)) ||
        !uart_send_text(" ref=") ||
        !uart_send_hex32(rd24(G_CAP_REF_COUNTER)) ||
        !uart_send_text(" wrap=") ||
        !uart_send_hex32(rd24(G_CAP_WRAP_COUNTER)) ||
        !uart_send_text(" el=") ||
        !uart_send_hex32(REG32(G_CAP_ELAPSED_TICKS)) ||
        !uart_send_text(" tw=") ||
        !uart_send_hex16(REG16(G_FRONTEND_TIMING_WORD)) ||
        !uart_send_text(" raw=") ||
        !uart_send_hex32(REG32(G_CAP_RAW_DISPLAY_VALUE)) ||
        !uart_send_text(" val=") ||
        !uart_send_hex32(REG32(G_MEASURE_VALUE)) ||
        !uart_send_text(" disp=") ||
        !uart_send_hex32(REG32(G_DISPLAY_VALUE)) ||
        !uart_send_text(" raw=") ||
        !uart_send_hex32(REG32(G_RAW_SAMPLE_LAST)) ||
        !uart_send_text(" zero=") ||
        !uart_send_hex32(REG32(G_CAL_ZERO_OFFSET_RAW)) ||
        !uart_send_text(" gain=") ||
        !uart_send_hex32(REG32(G_CAL_GAIN_FACTOR_FP)) ||
        !uart_send_text(" ticks=") ||
        !uart_send_hex16(REG16(G_CAP_TIMEOUT_TICKS)) ||
        !uart_send_text(" tc=") ||
        !uart_send_hex16(REG16(G_CAP_TIMEOUT_COUNTER)) ||
        !uart_send_text(" fl=") ||
        !uart_send_hex8((uint8_t)((REG8(G_CAP_TIMEOUT_PENDING) ? 1u : 0u) |
                             (REG8(G_RANGE_CHANGED_FLAG) ? 2u : 0u) |
                             (REG8(G_MEASUREMENT_READY_FOR_DISPLAY) ? 4u : 0u) |
                             (REG8(G_DISPLAY_OVERRANGE_FLAG) ? 8u : 0u) |
                             (REG8(G_MANUAL_RANGE_OR_HOLD_FLAG) ? 0x10u : 0u) |
                             (REG8(G_CAP_FORCE_LOW_RANGE_AFTER_TIMEOUT) ? 0x20u : 0u) |
                             (REG8(G_CAP_OPTION_TABLE_ENABLED) ? 0x40u : 0u))) ||
        !uart_send_text(" h=") ||
        !uart_send_hex8(REG8(G_CAP_MANUAL_RANGE4_HOLDOFF)) ||
        !uart_send_text("\r\n")) {
        return;
    }
}

static void diag_emit_cap_forced(void) {
    diag_state_init();
    uint8_t saved = REG8(DIAG_DEBUG_ENABLED_ADDR);
    REG8(DIAG_DEBUG_ENABLED_ADDR) = 1;
    diag_emit_cap('D');
    REG8(DIAG_DEBUG_ENABLED_ADDR) = saved;
}

static void diag_emit_p10_state(const char *tag) {
    uint8_t substate_flags = (uint8_t)((REG8(G_VOLTAGE_ALT_BANK_ACTIVE) ? 0x01u : 0u) |
                                       (REG8(G_VOLTAGE_ALT_SUBPATH_BD) ? 0x02u : 0u) |
                                       (REG8(G_MEASURE_ALT_PATH_B7) ? 0x04u : 0u) |
                                       (REG8(G_MEASURE_ALT_PATH_B9) ? 0x08u : 0u) |
                                       (REG8(G_MODE0_ALT_OR_REL_FLAG) ? 0x10u : 0u) |
                                       (REG8(G_FRONTEND_TIMING_TOGGLE_PHASE) ? 0x20u : 0u) |
                                       (REG8(G_FRONTEND_TIMING_TOGGLE_INHIBIT) ? 0x40u : 0u));
    uint8_t measure_flags = (uint8_t)((REG8(G_RANGE_CHANGED_FLAG) ? 1u : 0u) |
                                      (REG8(G_MEASUREMENT_READY_FOR_DISPLAY) ? 2u : 0u) |
                                      (REG8(G_DISPLAY_OVERRANGE_FLAG) ? 4u : 0u) |
                                      (REG8(G_MANUAL_RANGE_OR_HOLD_FLAG) ? 8u : 0u) |
                                      (REG8(G_BT_STREAMING_ENABLED) ? 0x10u : 0u));
    if (!uart_send_text(tag) ||
        !uart_send_text(" mode=") ||
        !uart_send_hex8(REG8(G_DMM_MODE)) ||
        !uart_send_text(" range=") ||
        !uart_send_hex8(REG8(G_DMM_RANGE)) ||
        !uart_send_text(" status=") ||
        !uart_send_hex8(REG8(G_FRONTEND_STATUS_MASK)) ||
        !uart_send_text(" r1e=") ||
        !uart_send_hex8(REG8(G_FRONTEND_EVENT_STATUS_1E)) ||
        !uart_send_text(" r1f=") ||
        !uart_send_hex8(REG8(G_FRONTEND_EVENT_MASK_1F)) ||
        !uart_send_text(" r20=") ||
        !uart_send_hex8(REG8(G_FRONTEND_RX_REG20)) ||
        !uart_send_text(" r29=") ||
        !uart_send_hex8(REG8(G_FRONTEND_RX_REG29)) ||
        !uart_send_text(" flags=") ||
        !uart_send_hex8(measure_flags) ||
        !uart_send_text(" sfl=") ||
        !uart_send_hex8(substate_flags) ||
        !uart_send_text(" cap_count=") ||
        !uart_send_hex32(rd24(G_CAP_COUNT_COUNTER)) ||
        !uart_send_text(" cap_ref=") ||
        !uart_send_hex32(rd24(G_CAP_REF_COUNTER)) ||
        !uart_send_text(" cap_wrap=") ||
        !uart_send_hex32(rd24(G_CAP_WRAP_COUNTER)) ||
        !uart_send_text(" cap_el=") ||
        !uart_send_hex32(REG32(G_CAP_ELAPSED_TICKS)) ||
        !uart_send_text(" tw=") ||
        !uart_send_hex16(REG16(G_FRONTEND_TIMING_WORD)) ||
        !uart_send_text(" cap_tw=") ||
        !uart_send_hex16(REG16(G_FRONTEND_TIMING_WORD)) ||
        !uart_send_text(" ticks=") ||
        !uart_send_hex16(REG16(G_CAP_TIMEOUT_TICKS)) ||
        !uart_send_text(" tc=") ||
        !uart_send_hex16(REG16(G_CAP_TIMEOUT_COUNTER)) ||
        !uart_send_text(" h=") ||
        !uart_send_hex8(REG8(G_CAP_MANUAL_RANGE4_HOLDOFF)) ||
        !uart_send_text(" cap_raw=") ||
        !uart_send_hex32(REG32(G_CAP_RAW_DISPLAY_VALUE)) ||
        !uart_send_text(" val=") ||
        !uart_send_hex32(REG32(G_MEASURE_VALUE)) ||
        !uart_send_text(" disp=") ||
        !uart_send_hex32(REG32(G_DISPLAY_VALUE)) ||
        !uart_send_text(" raw=") ||
        !uart_send_hex32(REG32(G_RAW_SAMPLE_LAST)) ||
        !uart_send_text(" av=") ||
        !uart_send_hex32(REG32(G_AVG_SAMPLE_VALUE)) ||
        !uart_send_text(" zero=") ||
        !uart_send_hex32(REG32(G_CAL_ZERO_OFFSET_RAW)) ||
        !uart_send_text(" gain=") ||
        !uart_send_hex32(REG32(G_CAL_GAIN_FACTOR_FP)) ||
        !uart_send_text(" fref=") ||
        !uart_send_hex32(REG32(G_FREQ_REFERENCE_CAL_HZ)) ||
        !uart_send_text(" fl=") ||
        !uart_send_hex8(measure_flags) ||
        !uart_send_text(" sf=") ||
        !uart_send_hex8(substate_flags) ||
        !uart_send_text(" rot=") ||
        !uart_send_hex8(REG8(G_ROTARY_STABLE_POSITION)) ||
        !uart_send_text(" db=") ||
        !uart_send_hex8(REG8(G_DUAL_BANK_ACTIVE)) ||
        !uart_send_text(" c6=") ||
        !uart_send_hex8(REG8(G_ALT_RANGE_INDEX_C6)) ||
        !uart_send_text(" c7=") ||
        !uart_send_hex8(REG8(G_ALT_RANGE_INDEX_C7)) ||
        !uart_send_text(" b0=") ||
        !uart_send_hex32(REG32(G_DUAL_BANK0_VALUE)) ||
        !uart_send_text(" b1=") ||
        !uart_send_hex32(REG32(G_DUAL_BANK1_VALUE)) ||
        (REG8(G_DMM_MODE) == 0x05u &&
         (!uart_send_text(" tval=") ||
          !uart_send_hex32(REG32(G_TEMP_CONVERTED_CENTIDEG)) ||
          !uart_send_text(" tu=") ||
          !uart_send_text(REG8(G_TEMPERATURE_UNIT_C_FLAG) ? "C" : "F") ||
          !uart_send_text(" cj=") ||
          !uart_send_hex32(REG32(G_TEMP_COLD_JUNCTION_EMF)))) ||
        !uart_send_text("\r\n")) {
        return;
    }
}

static void diag_emit_p10_short_state(void) {
    static const uint32_t mux_ports[] = {
        GPIOA_BASE, GPIOC_BASE, GPIOD_BASE, GPIOE_BASE, GPIOF_BASE, GPIOG_BASE,
    };
    uint8_t substate_flags = (uint8_t)((REG8(G_VOLTAGE_ALT_BANK_ACTIVE) ? 0x01u : 0u) |
                                       (REG8(G_VOLTAGE_ALT_SUBPATH_BD) ? 0x02u : 0u) |
                                       (REG8(G_MEASURE_ALT_PATH_B7) ? 0x04u : 0u) |
                                       (REG8(G_MEASURE_ALT_PATH_B9) ? 0x08u : 0u) |
                                       (REG8(G_MODE0_ALT_OR_REL_FLAG) ? 0x10u : 0u) |
                                       (REG8(G_FRONTEND_TIMING_TOGGLE_PHASE) ? 0x20u : 0u) |
                                       (REG8(G_FRONTEND_TIMING_TOGGLE_INHIBIT) ? 0x40u : 0u));
    if (!uart_send_text("UP10S m=") ||
        !uart_send_hex8(REG8(G_DMM_MODE)) ||
        !uart_send_text(" r=") ||
        !uart_send_hex8(REG8(G_DMM_RANGE)) ||
        !uart_send_text(" st=") ||
        !uart_send_hex8(REG8(G_FRONTEND_STATUS_MASK)) ||
        !uart_send_text(" r1f=") ||
        !uart_send_hex8(REG8(G_FRONTEND_EVENT_MASK_1F)) ||
        !uart_send_text(" r20=") ||
        !uart_send_hex8(REG8(G_FRONTEND_RX_REG20)) ||
        !uart_send_text(" tw=") ||
        !uart_send_hex16(REG16(G_FRONTEND_TIMING_WORD)) ||
        !uart_send_text(" sf=") ||
        !uart_send_hex8(substate_flags) ||
        !uart_send_text(" val=") ||
        !uart_send_hex32(REG32(G_MEASURE_VALUE)) ||
        !uart_send_text(" disp=") ||
        !uart_send_hex32(REG32(G_DISPLAY_VALUE)) ||
        !uart_send_text(" raw=") ||
        !uart_send_hex32(REG32(G_RAW_SAMPLE_LAST)) ||
        !uart_send_text(" avg=") ||
        !uart_send_hex32(REG32(G_AVG_SAMPLE_VALUE)) ||
        !uart_send_text(" cj=") ||
        !uart_send_hex32(REG32(G_TEMP_COLD_JUNCTION_EMF)) ||
        !uart_send_text(" tcg=") ||
        !uart_send_hex32(REG32(G_TEMP_CONVERTED_CENTIDEG)) ||
        !uart_send_text(" lcd=")) {
        return;
    }
    for (uint8_t i = 0; i < 4u; ++i) {
        if (i != 0u && !uart_send_char(',')) {
            return;
        }
        if (!uart_send_hex32(REG32(LCD_RAM_BASE + (uint32_t)i * 4u))) {
            return;
        }
    }
    if (!uart_send_text(" mux=")) {
        return;
    }
    for (uint8_t i = 0; i < (uint8_t)(sizeof(mux_ports) / sizeof(mux_ports[0])); ++i) {
        if (i != 0u && !uart_send_char(',')) {
            return;
        }
        if (!uart_send_hex16((uint16_t)REG32(GPIO_ODR(mux_ports[i])))) {
            return;
        }
    }
    uart_send_text("\r\n");
}

static void diag_emit_app_state(void) {
    uint8_t rotary = REG8(DIAG_ROTARY_OVERRIDE_ENABLED_ADDR) ?
                         REG8(DIAG_ROTARY_OVERRIDE_VALUE_ADDR) :
                         REG8(G_ROTARY_STABLE_POSITION);
    if (!uart_send_text("UAPP rot=") ||
        !uart_send_hex8(rotary) ||
        !uart_send_text(" btn=00 mode=") ||
        !uart_send_hex8(REG8(G_DMM_MODE)) ||
        !uart_send_text(" range=") ||
        !uart_send_hex8(REG8(G_DMM_RANGE)) ||
        !uart_send_text("\r\n")) {
        return;
    }
}

static void diag_emit_sim_state(uint8_t button_mask) {
    uint8_t enabled = REG8(DIAG_ROTARY_OVERRIDE_ENABLED_ADDR);
    uint8_t rotary = enabled ? REG8(DIAG_ROTARY_OVERRIDE_VALUE_ADDR) : REG8(G_ROTARY_STABLE_POSITION);
    if (!uart_send_text("UXIN en=") ||
        !uart_send_char(enabled ? '1' : '0') ||
        !uart_send_text(" rot=") ||
        !uart_send_hex8(rotary) ||
        !uart_send_text(" jack=00 btn=") ||
        !uart_send_hex8(button_mask) ||
        !uart_send_text("\r\n")) {
        return;
    }
}

static void diag_emit_lcd_ram(const char *tag) {
    if (!uart_send_text(tag)) {
        return;
    }
    for (uint8_t i = 0; i < 16u; ++i) {
        if (!uart_send_text(" ") ||
            !uart_send_hex32(REG32(LCD_RAM_BASE + (uint32_t)i * 4u))) {
            return;
        }
    }
    uart_send_text("\r\n");
}

static uint8_t frontend_ready_direct(void) {
    return (REG32(GPIO_IDR(GPIOG_BASE)) & BIT(3)) != 0u ? 1u : 0u;
}

static void diag_read_frontend_regs(uint8_t reg, uint8_t len) {
    STOCK_FRONTEND_READ_REGISTERS(0u, reg, len);
}

static void diag_emit_frontend_raw(void) {
    uint8_t ready_before = frontend_ready_direct();
    diag_read_frontend_regs(0, 52u);
    uint8_t ready_after = frontend_ready_direct();

    if (!uart_send_text("UFE ready=") ||
        !uart_send_hex8(ready_before) ||
        !uart_send_text("/") ||
        !uart_send_hex8(ready_after) ||
        !uart_send_text(" r1e=") ||
        !uart_send_hex8(REG8(G_FRONTEND_RX_REGS + 0x1Eu)) ||
        !uart_send_text(" r1f=") ||
        !uart_send_hex8(REG8(G_FRONTEND_RX_REGS + 0x1Fu)) ||
        !uart_send_text(" r20=") ||
        !uart_send_hex8(REG8(G_FRONTEND_RX_REGS + 0x20u)) ||
        !uart_send_text(" r29=") ||
        !uart_send_hex8(REG8(G_FRONTEND_RX_REGS + 0x29u)) ||
        !uart_send_text(" s=")) {
        return;
    }
    for (uint8_t i = 0; i < 6u; ++i) {
        if (!uart_send_hex8(REG8(G_FRONTEND_RX_REGS + i))) {
            return;
        }
    }
    if (!uart_send_text(" cap=")) {
        return;
    }
    for (uint8_t i = 0x15u; i <= 0x1Du; ++i) {
        if (!uart_send_hex8(REG8(G_FRONTEND_RX_REGS + i))) {
            return;
        }
    }
    uart_send_text("\r\n");
}

static void diag_emit_frontend_config(void) {
    diag_read_frontend_regs(0x20u, 20u);
    uint8_t cfg[20];
    for (uint8_t i = 0; i < 20u; ++i) {
        cfg[i] = REG8(G_FRONTEND_RX_REGS + i);
    }
    diag_read_frontend_regs(0x1Eu, 2u);
    uint8_t ctrl0 = REG8(G_FRONTEND_RX_REGS);
    uint8_t ctrl1 = REG8(G_FRONTEND_RX_REGS + 1u);

    if (!uart_send_text("UFECFG ctrl=") ||
        !uart_send_hex8(ctrl0) ||
        !uart_send_text("/") ||
        !uart_send_hex8(ctrl1) ||
        !uart_send_text(" cfg=")) {
        return;
    }
    for (uint8_t i = 0; i < 20u; ++i) {
        if (!uart_send_hex8(cfg[i])) {
            return;
        }
    }
    uart_send_text("\r\n");
}

static void diag_emit_frontend_dump(void) {
    uint8_t ready_before = frontend_ready_direct();
    diag_read_frontend_regs(0, 52u);
    uint8_t ready_after = frontend_ready_direct();

    if (!uart_send_text("UFEDUMP ready=") ||
        !uart_send_hex8(ready_before) ||
        !uart_send_text("/") ||
        !uart_send_hex8(ready_after) ||
        !uart_send_text(" regs=")) {
        return;
    }
    for (uint8_t i = 0; i < 52u; ++i) {
        if (!uart_send_hex8(REG8(G_FRONTEND_RX_REGS + i))) {
            return;
        }
    }
    uart_send_text("\r\n");
}

static void stock_sim_direct_mode_range(uint8_t mode, uint8_t range);

static uint8_t stock_rotary_for_mode(uint8_t mode) {
    if (mode == 0x00u) {
        return 0x01u;
    }
    if (mode == 0x01u || mode == 0x02u) {
        return 0x02u;
    }
    if (mode == 0x03u || mode == 0x04u || mode == 0x05u) {
        return 0x04u;
    }
    if (mode == 0x06u || mode == 0x07u || mode == 0x08u) {
        return 0x08u;
    }
    if (mode >= 0x09u && mode <= 0x0Cu) {
        return 0x10u;
    }
    if (mode == 0x0Fu || mode == 0x18u) {
        return 0x20u;
    }
    if (mode == 0x0Du || mode == 0x10u || mode == 0x11u || mode == 0x16u ||
        mode == 0x22u) {
        return 0x40u;
    }
    if (mode == 0x12u || mode == 0x13u || mode == 0x14u || mode == 0x15u) {
        return 0x80u;
    }
    return REG8(G_ROTARY_STABLE_POSITION);
}

static void stock_enforce_direct_p10_override(void) {
    if (REG8(DIAG_P10_OVERRIDE_ENABLED_ADDR) == 0u) {
        return;
    }
    uint8_t mode = REG8(DIAG_P10_OVERRIDE_MODE_ADDR);
    uint8_t range = REG8(DIAG_P10_OVERRIDE_RANGE_ADDR);
    REG8(G_DMM_MODE) = mode;
    REG8(G_DMM_RANGE) = range;
    REG8(G_ROTARY_STABLE_POSITION) = stock_rotary_for_mode(mode);
}

static void stock_sim_rotary(uint8_t position) {
    direct_uart_init_ble_off();
    REG8(G_BT_STREAMING_ENABLED) = 0;
    REG8(DIAG_P10_OVERRIDE_ENABLED_ADDR) = 0;
    REG8(DIAG_ROTARY_OVERRIDE_ENABLED_ADDR) = 1;
    REG8(DIAG_ROTARY_OVERRIDE_VALUE_ADDR) = position;
    REG8(G_ROTARY_STABLE_POSITION) = position;
    switch (position) {
    case 0x01:
    case 0x02:
    case 0x04:
    case 0x08:
    case 0x10:
    case 0x20:
    case 0x40:
    case 0x80:
        STOCK_ROTARY_APPLY_POSITION();
        break;
    default:
        STOCK_ROTARY_APPLY_POSITION();
        direct_uart_init_ble_off();
        REG8(G_BT_STREAMING_ENABLED) = 0;
        return;
    }
    REG8(G_MANUAL_RANGE_OR_HOLD_FLAG) = 0;
    REG8(G_DISPLAY_OVERRANGE_FLAG) = 0;
    REG8(G_MEASUREMENT_READY_FOR_DISPLAY) = 0;
    REG8(G_MODE_CHANGE_REQUIRES_MEASUREMENT_RESET) = 1;
    REG8(G_RANGE_CHANGED_FLAG) = 1;
    STOCK_DMM_RESET_MEASUREMENT_STATE(1u);
    REG8(G_ROTARY_STABLE_POSITION) = position;
    direct_uart_init_ble_off();
    REG8(G_BT_STREAMING_ENABLED) = 0;
}

uint8_t __attribute__((used)) hook_rotary_sample_override(void) {
    if (diag_enter_updater_if_pending()) {
        return REG8(G_ROTARY_STABLE_POSITION);
    }
    diag_apply_pending_direct_mode();
    if (REG8(DIAG_DIRECT_ACK_PENDING_ADDR) != 0u) {
        REG8(DIAG_DIRECT_ACK_PENDING_ADDR) = 0;
        uart_send_text("UP10SETOK pending=01 mode=");
        uart_send_hex8(REG8(DIAG_P10_OVERRIDE_MODE_ADDR));
        uart_send_text(" range=");
        uart_send_hex8(REG8(DIAG_P10_OVERRIDE_RANGE_ADDR));
        uart_send_text("\r\n");
    }
    stock_enforce_direct_p10_override();
    if (REG8(DIAG_ROTARY_OVERRIDE_ENABLED_ADDR) != 0u) {
        uint8_t value = REG8(DIAG_ROTARY_OVERRIDE_VALUE_ADDR);
        REG8(G_ROTARY_CANDIDATE_POSITION) = value;
        return value;
    }
    return STOCK_ROTARY_SAMPLE_GPIOH_ACTIVE_LOW();
}

static void stock_sim_mode_click(void) {
    direct_uart_init_ble_off();
    REG8(G_BT_STREAMING_ENABLED) = 0;
    REG8(DIAG_P10_OVERRIDE_ENABLED_ADDR) = 0;
    STOCK_MODE_CYCLE_SECONDARY();
    direct_uart_init_ble_off();
    REG8(G_BT_STREAMING_ENABLED) = 0;
}

static void stock_sim_key(uint8_t key_mask) {
    direct_uart_init_ble_off();
    REG8(G_BT_STREAMING_ENABLED) = 0;
    REG8(DIAG_P10_OVERRIDE_ENABLED_ADDR) = 0;
    STOCK_UI_HANDLE_KEY_EVENT(key_mask);
    direct_uart_init_ble_off();
    REG8(G_BT_STREAMING_ENABLED) = 0;
}

static void stock_sim_direct_mode_range(uint8_t mode, uint8_t range) {
    direct_uart_init_ble_off();
    REG8(G_BT_STREAMING_ENABLED) = 0;
    REG8(DIAG_P10_OVERRIDE_ENABLED_ADDR) = 0;
    uint8_t rotary = stock_rotary_for_mode(mode);
    REG8(DIAG_ROTARY_OVERRIDE_ENABLED_ADDR) = 1;
    REG8(DIAG_ROTARY_OVERRIDE_VALUE_ADDR) = rotary;
    REG8(G_ROTARY_STABLE_POSITION) = rotary;
    STOCK_ROTARY_APPLY_POSITION();
    for (uint8_t guard = 0; guard < 8u && REG8(G_DMM_MODE) != mode; ++guard) {
        STOCK_MODE_CYCLE_SECONDARY();
    }
    REG8(G_DMM_RANGE) = range;
    REG8(G_MANUAL_RANGE_OR_HOLD_FLAG) = 0;
    REG8(G_DISPLAY_OVERRANGE_FLAG) = 0;
    REG8(G_MEASUREMENT_READY_FOR_DISPLAY) = 0;
    REG8(G_MODE_CHANGE_REQUIRES_MEASUREMENT_RESET) = 1;
    REG8(G_RANGE_CHANGED_FLAG) = 1;
    STOCK_DMM_RESET_MEASUREMENT_STATE(1u);
    direct_uart_init_ble_off();
    REG8(G_BT_STREAMING_ENABLED) = 0;
}

static void diag_apply_pending_direct_mode(void) {
    if (REG8(DIAG_DIRECT_MODE_PENDING_ADDR) != 0xA5u) {
        return;
    }
    uint8_t mode = REG8(DIAG_P10_OVERRIDE_MODE_ADDR);
    uint8_t range = REG8(DIAG_P10_OVERRIDE_RANGE_ADDR);
    REG8(DIAG_DIRECT_MODE_PENDING_ADDR) = 0;
    stock_sim_direct_mode_range(mode, range);
}

void hook_cap_compute(void) {
    STOCK_CAP_COMPUTE();
    diag_emit_cap('E');
}

void hook_cap_timeout(void) {
    uint8_t pending_before = REG8(G_CAP_TIMEOUT_PENDING);
    diag_apply_pending_direct_mode();
    STOCK_CAP_TIMEOUT();
    if (pending_before != 0u) {
        diag_emit_cap('T');
    }
}

static bool starts_with(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return false;
        }
    }
    return true;
}

static uint8_t hex_value(char ch, bool *ok) {
    if (ch >= '0' && ch <= '9') {
        return (uint8_t)(ch - '0');
    }
    if (ch >= 'A' && ch <= 'F') {
        return (uint8_t)(ch - 'A' + 10);
    }
    if (ch >= 'a' && ch <= 'f') {
        return (uint8_t)(ch - 'a' + 10);
    }
    *ok = false;
    return 0;
}

static bool parse_hex32(const char **cursor, uint32_t *value) {
    bool ok = true;
    uint32_t out = 0;
    uint8_t count = 0;
    const char *p = *cursor;
    while ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')) {
        if (count >= 8u) {
            return false;
        }
        out = (out << 4) | hex_value(*p, &ok);
        ++p;
        ++count;
    }
    if (!ok || count == 0u) {
        return false;
    }
    *cursor = p;
    *value = out;
    return true;
}

static void skip_spaces(const char **cursor) {
    while (**cursor == ' ') {
        ++*cursor;
    }
}

static bool updater_baud_supported(uint32_t baud) {
    return baud == 19200u || baud == 38400u || baud == 57600u || baud == 115200u ||
           baud == 230400u || baud == 460800u || baud == 921600u;
}

static uint32_t parse_optional_data_baud(const char **cursor) {
    uint32_t baud = 19200u;
    skip_spaces(cursor);
    if (**cursor != '\0') {
        uint32_t parsed = 0;
        if (parse_hex32(cursor, &parsed) && updater_baud_supported(parsed)) {
            baud = parsed;
        } else {
            baud = 0;
        }
    }
    return baud;
}

static void RAMFUNC ram_stream_session(bool flash_mode, uint32_t initial_size, uint32_t initial_crc, uint32_t initial_data_baud);

static void enter_ram_stream(bool flash_mode, uint32_t size, uint32_t crc, uint32_t data_baud) {
    uint8_t *src = &__ramfunc_load;
    uint8_t *dst = &__ramfunc_start;
    while (dst < &__ramfunc_end) {
        *dst++ = *src++;
    }
    __asm__ volatile("cpsid i\n dsb\n isb" ::: "memory");
    void (*entry)(bool, uint32_t, uint32_t, uint32_t) =
        (void (*)(bool, uint32_t, uint32_t, uint32_t))(uintptr_t)((uint32_t)(uintptr_t)&ram_stream_session | 1u);
    entry(flash_mode, size, crc, data_baud);
    while (1) {
    }
}

static void diag_process_line(char *line) {
    if (line[0] != 'U') {
        return;
    }
    if (REG8(DIAG_DIRECT_MODE_PENDING_ADDR) == 0xA5u && !starts_with(line, "UP10SET")) {
        diag_apply_pending_direct_mode();
    }

    if ((line[1] == '?' && line[2] == '\0') || starts_with(line, "UINFO")) {
        uart_send_text("UINFO stockdiag=1 cap=1 upd=1 direct=1 ble=0 app=08006000 max=0005A000 chunk=00000020 debug=");
        uart_send_char(diag_debug_enabled() ? '1' : '0');
        uart_send_text("\r\n");
        return;
    }
    if (starts_with(line, "UCAPDUMP")) {
        diag_emit_cap_forced();
        return;
    }
    if (starts_with(line, "UAPP")) {
        diag_emit_app_state();
        return;
    }
    if (starts_with(line, "UP10D") || starts_with(line, "USTATE")) {
        diag_emit_p10_state("UP10D");
        return;
    }
    if (starts_with(line, "UP10SET")) {
        const char *p = &line[7];
        uint32_t mode = 0;
        uint32_t range = 0;
        skip_spaces(&p);
        if (!parse_hex32(&p, &mode) || mode > 0xFFu) {
            uart_send_text("UERR P10SET\r\n");
            return;
        }
        skip_spaces(&p);
        if (!parse_hex32(&p, &range) || range > 0xFFu) {
            uart_send_text("UERR P10SET\r\n");
            return;
        }
        REG8(DIAG_P10_OVERRIDE_MODE_ADDR) = (uint8_t)mode;
        REG8(DIAG_P10_OVERRIDE_RANGE_ADDR) = (uint8_t)range;
        REG8(DIAG_DIRECT_MODE_PENDING_ADDR) = 0xA5u;
        uart_send_text("UP10SETOK pending=01 mode=");
        uart_send_hex8((uint8_t)mode);
        uart_send_text(" range=");
        uart_send_hex8((uint8_t)range);
        uart_send_text("\r\n");
        return;
    }
    if (starts_with(line, "UP10S")) {
        diag_emit_p10_short_state();
        return;
    }
    if (starts_with(line, "ULCDR")) {
        diag_emit_lcd_ram("ULCDR");
        return;
    }
    if (starts_with(line, "ULCD")) {
        diag_emit_lcd_ram("ULCD");
        return;
    }
    if (starts_with(line, "UFECFG")) {
        diag_emit_frontend_config();
        return;
    }
    if (starts_with(line, "UFEDUMP")) {
        diag_emit_frontend_dump();
        return;
    }
    if (starts_with(line, "UFE")) {
        diag_emit_frontend_raw();
        return;
    }
    if (starts_with(line, "UTOVR")) {
        const char *p = &line[5];
        uint32_t parsed = 0;
        skip_spaces(&p);
        if (!parse_hex32(&p, &parsed)) {
            uart_send_text("UERR TOVR\r\n");
            return;
        }
        if (parsed == 0xFFFFFFFFu) {
            REG8(DIAG_TEMP_OVERRIDE_ENABLED_ADDR) = 0;
        } else {
            REG16(DIAG_TEMP_OVERRIDE_TENTHS_ADDR) = (uint16_t)parsed;
            REG8(DIAG_TEMP_OVERRIDE_ENABLED_ADDR) = 1;
            diag_apply_temperature_override();
        }
        skip_spaces(&p);
        if (*p != '\0') {
            uint32_t battery = 0;
            if (!parse_hex32(&p, &battery)) {
                uart_send_text("UERR TOVR\r\n");
                return;
            }
            if (battery == 0xFFu || battery == 0xFFFFFFFFu) {
                REG8(DIAG_BAT_OVERRIDE_ENABLED_ADDR) = 0;
            } else if (battery <= 99u) {
                REG8(DIAG_BAT_OVERRIDE_PERCENT_ADDR) = (uint8_t)battery;
                REG8(DIAG_BAT_OVERRIDE_ENABLED_ADDR) = 1;
                skip_spaces(&p);
                if (*p != '\0') {
                    uint32_t low = 0;
                    if (!parse_hex32(&p, &low) || low > 1u) {
                        uart_send_text("UERR TOVR\r\n");
                        return;
                    }
                    REG8(DIAG_BAT_OVERRIDE_LOW_ADDR) = (uint8_t)low;
                }
                diag_apply_battery_override();
            } else {
                uart_send_text("UERR TOVR\r\n");
                return;
            }
        }
        uart_send_text("UTOVR en=");
        uart_send_hex8(REG8(DIAG_TEMP_OVERRIDE_ENABLED_ADDR));
        uart_send_text(" tenths=");
        uart_send_hex16(REG16(DIAG_TEMP_OVERRIDE_TENTHS_ADDR));
        uart_send_text(" bat_en=");
        uart_send_hex8(REG8(DIAG_BAT_OVERRIDE_ENABLED_ADDR));
        uart_send_text(" bat_pct=");
        uart_send_hex8(REG8(DIAG_BAT_OVERRIDE_PERCENT_ADDR));
        uart_send_text(" bat_low=");
        uart_send_hex8(REG8(DIAG_BAT_OVERRIDE_LOW_ADDR));
        uart_send_text("\r\n");
        return;
    }
    if (starts_with(line, "UBOVR")) {
        const char *p = &line[5];
        uint32_t parsed = 0;
        skip_spaces(&p);
        if (!parse_hex32(&p, &parsed)) {
            uart_send_text("UERR BOVR\r\n");
            return;
        }
        if (parsed == 0xFFu || parsed == 0xFFFFFFFFu) {
            REG8(DIAG_BAT_OVERRIDE_ENABLED_ADDR) = 0;
        } else if (parsed <= 99u) {
            REG8(DIAG_BAT_OVERRIDE_PERCENT_ADDR) = (uint8_t)parsed;
            REG8(DIAG_BAT_OVERRIDE_ENABLED_ADDR) = 1;
            skip_spaces(&p);
            if (*p != '\0') {
                uint32_t low = 0;
                if (!parse_hex32(&p, &low) || low > 1u) {
                    uart_send_text("UERR BOVR\r\n");
                    return;
                }
                REG8(DIAG_BAT_OVERRIDE_LOW_ADDR) = (uint8_t)low;
            }
            diag_apply_battery_override();
        } else {
            uart_send_text("UERR BOVR\r\n");
            return;
        }
        uart_send_text("UBOVR en=");
        uart_send_hex8(REG8(DIAG_BAT_OVERRIDE_ENABLED_ADDR));
        uart_send_text(" pct=");
        uart_send_hex8(REG8(DIAG_BAT_OVERRIDE_PERCENT_ADDR));
        uart_send_text(" low=");
        uart_send_hex8(REG8(DIAG_BAT_OVERRIDE_LOW_ADDR));
        uart_send_text("\r\n");
        return;
    }
    if (starts_with(line, "UTUNIT")) {
        const char *p = &line[6];
        uint32_t parsed = 0;
        skip_spaces(&p);
        if (!parse_hex32(&p, &parsed)) {
            uart_send_text("UERR TUNIT\r\n");
            return;
        }
        if (parsed == 0u || parsed == 1u) {
            REG8(G_TEMPERATURE_UNIT_C_FLAG) = (uint8_t)parsed;
        } else if (parsed != 0xFFu && parsed != 0xFFFFFFFFu) {
            uart_send_text("UERR TUNIT\r\n");
            return;
        }
        uart_send_text("UTUNIT en=01 unit=");
        uart_send_text(REG8(G_TEMPERATURE_UNIT_C_FLAG) ? "C" : "F");
        uart_send_text("\r\n");
        return;
    }
    if (starts_with(line, "UTEMP")) {
        uart_send_text("UTEMP raw=");
        uart_send_hex16(REG16(G_TEMP_ADC_LATEST_RAW));
        uart_send_text(" avg=");
        uart_send_hex16(REG16(G_TEMP_ADC_AVG_RAW));
        uart_send_text(" aux=");
        uart_send_hex16(REG16(G_AUX_ADC_LATEST_RAW));
        uart_send_text(" cnt=");
        uart_send_hex8(REG8(G_TEMP_ADC_FILTER_COUNT));
        uart_send_text(" centi=");
        uart_send_hex16(REG16(G_TEMP_CONVERTED_CENTIDEG));
        uart_send_text(" cal=");
        uart_send_hex16(REG16(G_TEMP_CAL_OFFSET_CENTIDEG));
        uart_send_text(" unit=");
        uart_send_text(REG8(G_TEMPERATURE_UNIT_C_FLAG) ? "C" : "F");
        uart_send_text(" oor=");
        uart_send_hex8(REG8(G_TEMP_ADC_OUT_OF_RANGE));
        uart_send_text("\r\n");
        return;
    }
    if (starts_with(line, "UBAT")) {
        uart_send_text("UBAT pct=");
        uart_send_hex8(REG8(G_BATTERY_PERCENT));
        uart_send_text(" low=");
        uart_send_hex8(REG8(G_BATTERY_LOW_LATCHED));
        uart_send_text(" cand=");
        uart_send_hex8(REG8(G_BATTERY_LOW_CANDIDATE));
        uart_send_text(" deb=");
        uart_send_hex8(REG8(G_BATTERY_LOW_DEBOUNCE_COUNT));
        uart_send_text(" ovr=");
        uart_send_hex8(REG8(DIAG_BAT_OVERRIDE_ENABLED_ADDR));
        uart_send_text(" ovr_pct=");
        uart_send_hex8(REG8(DIAG_BAT_OVERRIDE_PERCENT_ADDR));
        uart_send_text(" ovr_low=");
        uart_send_hex8(REG8(DIAG_BAT_OVERRIDE_LOW_ADDR));
        uart_send_text("\r\n");
        return;
    }
    if (starts_with(line, "UXROT")) {
        const char *p = &line[5];
        uint32_t position = 0;
        skip_spaces(&p);
        if (!parse_hex32(&p, &position) || position > 0xFFu) {
            uart_send_text("UERR SIMROT\r\n");
            return;
        }
        stock_sim_rotary((uint8_t)position);
        diag_emit_sim_state(0u);
        return;
    }
    if (starts_with(line, "UROT")) {
        const char *p = &line[4];
        uint32_t position = 0;
        skip_spaces(&p);
        if (!parse_hex32(&p, &position) || position > 0xFFu) {
            uart_send_text("UERR ROT\r\n");
            return;
        }
        stock_sim_rotary((uint8_t)position);
        diag_emit_p10_state("UROTOK");
        return;
    }
    if (starts_with(line, "UXBTN")) {
        const char *p = &line[5];
        uint32_t button_mask = 0;
        skip_spaces(&p);
        if (!parse_hex32(&p, &button_mask) || button_mask > 0xFFu) {
            uart_send_text("UERR SIMBTN\r\n");
            return;
        }
        if (button_mask == 0x01u) {
            stock_sim_mode_click();
        } else if (button_mask != 0u) {
            stock_sim_key((uint8_t)button_mask);
        }
        diag_emit_sim_state((uint8_t)button_mask);
        return;
    }
    if (starts_with(line, "UXJACK")) {
        diag_emit_sim_state(0u);
        return;
    }
    if (starts_with(line, "UXCLR")) {
        REG8(DIAG_ROTARY_OVERRIDE_ENABLED_ADDR) = 0;
        REG8(DIAG_ROTARY_OVERRIDE_VALUE_ADDR) = 0;
        REG8(DIAG_P10_OVERRIDE_ENABLED_ADDR) = 0;
        diag_emit_sim_state(0u);
        return;
    }
    if (starts_with(line, "UXIN")) {
        diag_emit_sim_state(0u);
        return;
    }
    if (starts_with(line, "UMODE")) {
        stock_sim_mode_click();
        diag_emit_p10_state("UMODEOK");
        return;
    }
    if (starts_with(line, "UKEY")) {
        const char *p = &line[4];
        uint32_t key_mask = 0;
        skip_spaces(&p);
        if (!parse_hex32(&p, &key_mask) || key_mask > 0xFFu) {
            uart_send_text("UERR KEY\r\n");
            return;
        }
        stock_sim_key((uint8_t)key_mask);
        diag_emit_p10_state("UKEYOK");
        return;
    }
    if (starts_with(line, "USTOPHELLO") || starts_with(line, "UHELLO 0")) {
        REG8(DIAG_HELLO_ENABLED_ADDR) = 0;
        uart_send_text("UHELLO 0\r\n");
        return;
    }
    if (starts_with(line, "USTARTHELLO") || starts_with(line, "UHELLO 1")) {
        REG8(DIAG_HELLO_ENABLED_ADDR) = 1;
        uart_send_text("UHELLO 1\r\n");
        return;
    }
    if (starts_with(line, "USTOPDBG") || starts_with(line, "UDEBUG 0") || starts_with(line, "UDBG 0")) {
        REG8(DIAG_DEBUG_ENABLED_ADDR) = 0;
        uart_send_text("UDBG 0\r\n");
        return;
    }
    if (starts_with(line, "USTARTDBG") || starts_with(line, "UDEBUG 1") || starts_with(line, "UDBG 1")) {
        REG8(DIAG_DEBUG_ENABLED_ADDR) = 1;
        uart_send_text("UDBG 1\r\n");
        return;
    }

    bool flash_mode = false;
    const char *p = 0;
    if (starts_with(line, "UFBEGIN")) {
        flash_mode = true;
        p = &line[7];
    } else if (starts_with(line, "USIM")) {
        flash_mode = false;
        p = &line[4];
    } else {
        return;
    }

    uint32_t size = 0;
    uint32_t crc = 0;
    uint32_t data_baud = 19200u;
    skip_spaces(&p);
    if (!parse_hex32(&p, &size)) {
        uart_send_text("UERR BEGIN\r\n");
        return;
    }
    skip_spaces(&p);
    if (!parse_hex32(&p, &crc) || size == 0u || size > UPD_APP_MAX_SIZE || (size & 3u) != 0u) {
        uart_send_text("UERR SIZE\r\n");
        return;
    }
    data_baud = parse_optional_data_baud(&p);
    if (data_baud == 0u) {
        uart_send_text("UERR BAUD\r\n");
        return;
    }

    while ((REG32(USART_SR(USART2_BASE)) & BIT(6)) == 0u) {
    }
    enter_ram_stream(flash_mode, size, crc, data_baud);
}

static bool diag_stage_updater_start(char *line) {
    if (line[0] != 'U') {
        return false;
    }

    bool flash_mode = false;
    const char *p = 0;
    if (starts_with(line, "UFBEGIN")) {
        flash_mode = true;
        p = &line[7];
    } else if (starts_with(line, "USIM")) {
        flash_mode = false;
        p = &line[4];
    } else {
        return false;
    }

    uint32_t size = 0;
    uint32_t crc = 0;
    uint32_t data_baud = 19200u;
    skip_spaces(&p);
    if (!parse_hex32(&p, &size)) {
        return false;
    }
    skip_spaces(&p);
    if (!parse_hex32(&p, &crc) || size == 0u || size > UPD_APP_MAX_SIZE || (size & 3u) != 0u) {
        return false;
    }
    data_baud = parse_optional_data_baud(&p);
    if (data_baud == 0u) {
        return false;
    }

    REG32(DIAG_UPD_SIZE_ADDR) = size;
    REG32(DIAG_UPD_CRC_ADDR) = crc;
    REG32(DIAG_UPD_DATA_BAUD_ADDR) = data_baud;
    REG8(DIAG_UPD_FLASH_MODE_ADDR) = flash_mode ? 1u : 0u;
    REG8(DIAG_UPD_PENDING_ADDR) = 1u;
    REG8(G_BT_RX_FRAME_READY) = 1u;
    return true;
}

static void diag_rx_byte(uint8_t byte) {
    diag_state_init();

    char *buf = (char *)(uintptr_t)DIAG_RX_BUF_ADDR;
    uint8_t len = REG8(DIAG_RX_LEN_ADDR);

    if (byte == '\r') {
        return;
    }
    if (byte == '\n') {
        buf[len] = '\0';
        if (starts_with(buf, "UFBEGIN") || starts_with(buf, "USIM")) {
            (void)diag_stage_updater_start(buf);
            REG8(DIAG_RX_LEN_ADDR) = 0;
            return;
        }
        if (starts_with(buf, "UP10SET")) {
            const char *p = &buf[7];
            uint32_t mode = 0;
            uint32_t range = 0;
            skip_spaces(&p);
            if (parse_hex32(&p, &mode) && mode <= 0xFFu) {
                skip_spaces(&p);
                if (parse_hex32(&p, &range) && range <= 0xFFu) {
                    REG8(DIAG_P10_OVERRIDE_MODE_ADDR) = (uint8_t)mode;
                    REG8(DIAG_P10_OVERRIDE_RANGE_ADDR) = (uint8_t)range;
                    REG8(DIAG_DIRECT_MODE_PENDING_ADDR) = 0xA5u;
                    diag_apply_pending_direct_mode();
                    uart_send_text("UP10SETOK pending=01 mode=");
                    uart_send_hex8((uint8_t)mode);
                    uart_send_text(" range=");
                    uart_send_hex8((uint8_t)range);
                    uart_send_text("\r\n");
                }
            }
            REG8(DIAG_RX_LEN_ADDR) = 0;
            return;
        }
        if (starts_with(buf, "UTOVR") || starts_with(buf, "UBOVR") || starts_with(buf, "UTUNIT")) {
            diag_process_line(buf);
            REG8(DIAG_RX_LEN_ADDR) = 0;
            return;
        }
        if (!diag_stage_updater_start(buf) && buf[0] == 'U') {
            REG8(DIAG_RX_PENDING_ADDR) = 1u;
            REG8(G_BT_RX_FRAME_READY) = 1u;
        }
        REG8(DIAG_RX_LEN_ADDR) = 0;
        return;
    }
    if (byte < 0x20u || byte > 0x7Eu) {
        REG8(DIAG_RX_LEN_ADDR) = 0;
        return;
    }
    if (len == 0u && byte != 'U') {
        return;
    }
    if (len >= (DIAG_LINE_MAX - 1u)) {
        REG8(DIAG_RX_LEN_ADDR) = 0;
        uart_send_text("UERR LINE\r\n");
        return;
    }
    buf[len++] = (char)byte;
    REG8(DIAG_RX_LEN_ADDR) = len;
}

uint16_t hook_usart_read_data(uint32_t base) {
    uint16_t value = STOCK_USART_READ_DATA(base);
    diag_rx_byte((uint8_t)value);
    return value;
}

void hook_bt_process_rx_start_updater(void) {
    diag_state_init();
    if (REG8(DIAG_UPD_PENDING_ADDR) != 0u) {
        bool flash_mode = REG8(DIAG_UPD_FLASH_MODE_ADDR) != 0u;
        uint32_t size = REG32(DIAG_UPD_SIZE_ADDR);
        uint32_t crc = REG32(DIAG_UPD_CRC_ADDR);
        uint32_t data_baud = REG32(DIAG_UPD_DATA_BAUD_ADDR);
        REG8(DIAG_UPD_PENDING_ADDR) = 0;
        REG8(G_BT_RX_FRAME_READY) = 0;
        (void)uart_send_text("UPDSTART\r\n");
        enter_ram_stream(flash_mode, size, crc, data_baud);
    }
    if (REG8(DIAG_RX_PENDING_ADDR) != 0u) {
        REG8(DIAG_RX_PENDING_ADDR) = 0;
        REG8(G_BT_RX_FRAME_READY) = 0;
        diag_process_line((char *)(uintptr_t)DIAG_RX_BUF_ADDR);
        return;
    }
    STOCK_BT_PROCESS_RX_COMMAND_FRAME();
}

static uint32_t RAMFUNC ram_crc32_byte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (uint8_t i = 0; i < 8u; ++i) {
        crc = (crc & 1u) != 0u ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
    return crc;
}

static char RAMFUNC ram_hex_digit(uint8_t value) {
    value &= 0x0Fu;
    if (value < 10u) {
        return (char)((uint8_t)'0' + value);
    }
    return (char)((uint8_t)'A' + value - 10u);
}

static uint8_t RAMFUNC ram_hex_value(char ch, bool *ok) {
    if (ch >= '0' && ch <= '9') {
        return (uint8_t)(ch - '0');
    }
    if (ch >= 'A' && ch <= 'F') {
        return (uint8_t)(ch - 'A' + 10);
    }
    if (ch >= 'a' && ch <= 'f') {
        return (uint8_t)(ch - 'a' + 10);
    }
    *ok = false;
    return 0;
}

static bool RAMFUNC ram_parse_hex32(const char **cursor, uint32_t *value) {
    bool ok = true;
    uint32_t out = 0;
    uint8_t count = 0;
    const char *p = *cursor;
    while ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')) {
        if (count >= 8u) {
            return false;
        }
        out = (out << 4) | ram_hex_value(*p, &ok);
        ++p;
        ++count;
    }
    if (!ok || count == 0u) {
        return false;
    }
    *cursor = p;
    *value = out;
    return true;
}

static void RAMFUNC ram_send_char(char ch) {
    while ((REG32(USART_SR(USART2_BASE)) & BIT(7)) == 0u) {
    }
    REG32(USART_DR(USART2_BASE)) = (uint8_t)ch;
}

static void RAMFUNC ram_send_hex32(uint32_t value) {
    for (uint8_t i = 0; i < 8u; ++i) {
        ram_send_char(ram_hex_digit((uint8_t)(value >> (28u - i * 4u))));
    }
}

static void RAMFUNC ram_send_crlf(void) {
    ram_send_char('\r');
    ram_send_char('\n');
}

static void RAMFUNC ram_delay_cycles(uint32_t count) {
    while (count-- != 0u) {
        __asm volatile("nop");
    }
}

static uint32_t RAMFUNC ram_brr_for_baud(uint32_t baud) {
    if (baud == 921600u) {
        return USART2_BRR_FLASH_921600;
    }
    if (baud == 460800u) {
        return USART2_BRR_FLASH_460800;
    }
    if (baud == 230400u) {
        return USART2_BRR_FLASH_230400;
    }
    if (baud == 115200u) {
        return USART2_BRR_FLASH_115200;
    }
    if (baud == 57600u) {
        return USART2_BRR_FLASH_57600;
    }
    if (baud == 38400u) {
        return USART2_BRR_FLASH_38400;
    }
    return USART2_BRR_STOCK_19200;
}

static void RAMFUNC ram_uart_set_baud(uint32_t baud) {
    if (baud == 19200u) {
        return;
    }
    while ((REG32(USART_SR(USART2_BASE)) & BIT(6)) == 0u) {
    }
    uint32_t cr1 = REG32(USART_CR1(USART2_BASE));
    REG32(USART_CR1(USART2_BASE)) = cr1 & ~BIT(13);
    REG32(USART_BRR(USART2_BASE)) = ram_brr_for_baud(baud);
    REG32(USART_CR1(USART2_BASE)) = cr1;
    ram_delay_cycles(600000u);
}

static void RAMFUNC ram_send_code(uint32_t code, uint8_t len) {
    for (uint8_t i = 0; i < len; ++i) {
        ram_send_char((char)(code >> (i * 8u)));
    }
}

#define CODE4(a, b, c, d) ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

static void RAMFUNC ram_reply_err(uint32_t code, uint8_t len) {
    ram_send_code(CODE4('U', 'E', 'R', 'R'), 4);
    ram_send_char(' ');
    ram_send_code(code, len);
    ram_send_crlf();
}

static void RAMFUNC ram_send_ok(uint32_t code, uint8_t len) {
    ram_send_code(CODE4('U', 'O', 'K', ' '), 4);
    ram_send_code(code, len);
    ram_send_crlf();
}

static void RAMFUNC ram_send_header(bool flash_mode, uint32_t expected_size, uint32_t expected_crc) {
    ram_send_code(CODE4('U', 'R', 'A', 'M'), 4);
    ram_send_char(' ');
    if (flash_mode) {
        ram_send_code(CODE4('F', 'L', 'A', 'S'), 4);
        ram_send_char('H');
    } else {
        ram_send_code(CODE4('S', 'I', 'M', ' '), 3);
    }
    ram_send_char(' ');
    ram_send_code(CODE4('s', 'i', 'z', 'e'), 4);
    ram_send_char('=');
    ram_send_hex32(expected_size);
    ram_send_char(' ');
    ram_send_code(CODE4('c', 'r', 'c', '='), 4);
    ram_send_hex32(expected_crc);
    ram_send_char(' ');
    ram_send_code(CODE4('b', 'o', 'o', 't'), 4);
    ram_send_code(CODE4('l', 'o', 'c', 'k'), 4);
    ram_send_char('=');
    ram_send_code(CODE4('0', '8', '0', '0'), 4);
    ram_send_code(CODE4('0', '0', '0', '0'), 4);
    ram_send_char('-');
    ram_send_code(CODE4('0', '8', '0', '0'), 4);
    ram_send_code(CODE4('5', 'F', 'F', 'F'), 4);
    ram_send_crlf();
}

static void RAMFUNC ram_send_label_hex(uint32_t label, uint8_t len, uint32_t value) {
    ram_send_char(' ');
    ram_send_code(label, len);
    ram_send_char('=');
    ram_send_hex32(value);
}

static void RAMFUNC ram_send_clock_info(uint32_t data_baud) {
    ram_send_code(CODE4('U', 'C', 'L', 'K'), 4);
    ram_send_label_hex(CODE4('b', 'a', 'u', 'd'), 4, data_baud);
    ram_send_label_hex(CODE4('r', 'c', 'c', 'r'), 4, REG32(RCC_CR));
    ram_send_label_hex(CODE4('i', 'c', 's', 'c'), 4, REG32(RCC_ICSCR));
    ram_send_label_hex(CODE4('c', 'f', 'g', 'r'), 4, REG32(RCC_CFGR));
    ram_send_label_hex(CODE4('c', 'i', 'r', ' '), 3, REG32(RCC_CIR));
    ram_send_label_hex(CODE4('a', 'h', 'b', 'e'), 4, REG32(RCC_AHBENR));
    ram_send_label_hex(CODE4('a', 'p', 'b', '1'), 4, REG32(RCC_APB1ENR));
    ram_send_label_hex(CODE4('a', 'p', 'b', '2'), 4, REG32(RCC_APB2ENR));
    ram_send_label_hex(CODE4('b', 'r', 'r', ' '), 3, REG32(USART_BRR(USART2_BASE)));
    ram_send_label_hex(CODE4('c', 'r', '1', ' '), 3, REG32(USART_CR1(USART2_BASE)));
    ram_send_label_hex(CODE4('c', 'r', '2', ' '), 3, REG32(USART_CR2(USART2_BASE)));
    ram_send_label_hex(CODE4('c', 'r', '3', ' '), 3, REG32(USART_CR3(USART2_BASE)));
    ram_send_label_hex(CODE4('s', 'r', ' ', ' '), 2, REG32(USART_SR(USART2_BASE)));
    ram_send_crlf();
}

static void RAMFUNC ram_send_switch_baud(uint32_t data_baud) {
    if (data_baud == 19200u) {
        return;
    }
    ram_send_code(CODE4('U', 'S', 'W', 'I'), 4);
    ram_send_code(CODE4('T', 'C', 'H', ' '), 4);
    ram_send_code(CODE4('b', 'a', 'u', 'd'), 4);
    ram_send_char('=');
    ram_send_hex32(data_baud);
    ram_send_crlf();
    while ((REG32(USART_SR(USART2_BASE)) & BIT(6)) == 0u) {
    }
    ram_delay_cycles(80000u);
}

static void RAMFUNC ram_send_ack(uint32_t received) {
    ram_send_code(CODE4('U', 'A', 'C', 'K'), 4);
    ram_send_char(' ');
    ram_send_hex32(received);
    ram_send_crlf();
}

static void RAMFUNC ram_send_sim_ok(uint32_t erased_pages, uint32_t expected_size, uint32_t final_crc) {
    ram_send_code(CODE4('U', 'F', 'S', 'I'), 4);
    ram_send_code(CODE4('M', 'O', 'K', ' '), 4);
    ram_send_code(CODE4('p', 'a', 'g', 'e'), 4);
    ram_send_char('s');
    ram_send_char('=');
    ram_send_hex32(erased_pages);
    ram_send_char(' ');
    ram_send_code(CODE4('s', 'i', 'z', 'e'), 4);
    ram_send_char('=');
    ram_send_hex32(expected_size);
    ram_send_char(' ');
    ram_send_code(CODE4('c', 'r', 'c', '='), 4);
    ram_send_hex32(final_crc);
    ram_send_char(' ');
    ram_send_code(CODE4('r', 'e', 's', 't'), 4);
    ram_send_code(CODE4('a', 'r', 't', '='), 4);
    ram_send_code(CODE4('U', 'B', '\r', '\n'), 4);
}

static void RAMFUNC ram_send_flash_ok(uint32_t final_crc) {
    ram_send_code(CODE4('U', 'F', 'L', 'A'), 4);
    ram_send_code(CODE4('S', 'H', 'O', 'K'), 4);
    ram_send_char(' ');
    ram_send_code(CODE4('r', 'e', 's', 'e'), 4);
    ram_send_char('t');
    ram_send_char(' ');
    ram_send_code(CODE4('c', 'r', 'c', '='), 4);
    ram_send_hex32(final_crc);
    ram_send_crlf();
}

static uint8_t RAMFUNC ram_recv_byte(void) {
    while ((REG32(USART_SR(USART2_BASE)) & BIT(5)) == 0u) {
    }
    return (uint8_t)REG32(USART_DR(USART2_BASE));
}

static bool RAMFUNC ram_read_line(char *line, uint16_t max_len) {
    uint16_t len = 0;
    while (1) {
        uint8_t byte = ram_recv_byte();
        if (byte == '\r') {
            continue;
        }
        if (byte == '\n') {
            line[len] = '\0';
            return true;
        }
        if (byte < 0x20u || byte > 0x7Eu || len >= (uint16_t)(max_len - 1u)) {
            line[0] = '\0';
            return false;
        }
        line[len++] = (char)byte;
    }
}

static bool RAMFUNC ram_vector_plausible(const uint8_t first[16], uint8_t first_count) {
    if (first_count < 8u) {
        return false;
    }
    uint32_t stack = first[0] | ((uint32_t)first[1] << 8) | ((uint32_t)first[2] << 16) | ((uint32_t)first[3] << 24);
    uint32_t reset = first[4] | ((uint32_t)first[5] << 8) | ((uint32_t)first[6] << 16) | ((uint32_t)first[7] << 24);
    return stack >= UPD_RAM_MIN && stack <= UPD_RAM_MAX && (stack & 3u) == 0u &&
           reset >= APP_FLASH_BASE && reset < (APP_FLASH_BASE + UPD_APP_MAX_SIZE) && (reset & 1u) != 0u;
}

static uint32_t RAMFUNC ram_flash_unlock(void) {
    while ((REG32(FLASH_SR) & FLASH_SR_BSY) != 0u) {
    }
    if ((REG32(FLASH_PECR) & FLASH_PECR_PELOCK) != 0u) {
        REG32(FLASH_PEKEYR) = 0x89ABCDEFu;
        REG32(FLASH_PEKEYR) = 0x02030405u;
    }
    if ((REG32(FLASH_PECR) & FLASH_PECR_PRGLOCK) != 0u) {
        REG32(FLASH_PRGKEYR) = 0x8C9DAEBFu;
        REG32(FLASH_PRGKEYR) = 0x13141516u;
    }
    REG32(FLASH_SR) = FLASH_SR_EOP | FLASH_SR_ERRORS;
    return (REG32(FLASH_PECR) & (FLASH_PECR_PELOCK | FLASH_PECR_PRGLOCK)) == 0u ? 0u : 1u;
}

static uint32_t RAMFUNC ram_erase_page(uint32_t addr) {
    if (addr < APP_FLASH_BASE || addr >= (APP_FLASH_BASE + UPD_APP_MAX_SIZE) || (addr & (UPD_FLASH_PAGE_SIZE - 1u)) != 0u) {
        return 1u;
    }
    REG32(FLASH_SR) = FLASH_SR_EOP | FLASH_SR_ERRORS;
    REG32(FLASH_PECR) |= FLASH_PECR_ERASE | FLASH_PECR_PROG;
    REG32(addr) = 0u;
    while ((REG32(FLASH_SR) & FLASH_SR_BSY) != 0u) {
    }
    REG32(FLASH_PECR) &= ~(FLASH_PECR_ERASE | FLASH_PECR_PROG);
    return (REG32(FLASH_SR) & FLASH_SR_ERRORS) == 0u ? 0u : 2u;
}

static uint32_t RAMFUNC ram_flash_wait(void) {
    uint32_t guard = 0x8000u;
    while ((REG32(FLASH_SR) & FLASH_SR_BSY) != 0u && guard != 0u) {
        --guard;
    }
    if (guard == 0u) {
        return 5u;
    }
    if ((REG32(FLASH_SR) & FLASH_SR_WRPERR) != 0u) {
        return 2u;
    }
    if ((REG32(FLASH_SR) & FLASH_SR_ERRORS) != 0u) {
        return 3u;
    }
    return 4u;
}

static uint32_t RAMFUNC ram_program_half_page(uint32_t offset, const uint8_t half[UPD_FLASH_HALF_PAGE_SIZE]) {
    if ((offset & (UPD_FLASH_HALF_PAGE_SIZE - 1u)) != 0u || offset > UPD_APP_MAX_SIZE ||
        UPD_FLASH_HALF_PAGE_SIZE > (UPD_APP_MAX_SIZE - offset)) {
        return 1u;
    }
    if (ram_flash_wait() != 4u) {
        return 2u;
    }

    uint32_t actlr = REG32(SCB_ACTLR);
    REG32(SCB_ACTLR) = actlr | 1u;
    REG32(FLASH_SR) = FLASH_SR_EOP | FLASH_SR_ERRORS;
    REG32(FLASH_PECR) |= FLASH_PECR_FPRG;
    REG32(FLASH_PECR) |= FLASH_PECR_PROG;

    for (uint8_t i = 0; i < UPD_FLASH_HALF_PAGE_SIZE; i += 4u) {
        uint32_t word = half[i] | ((uint32_t)half[i + 1u] << 8) |
                        ((uint32_t)half[i + 2u] << 16) | ((uint32_t)half[i + 3u] << 24);
        REG32(APP_FLASH_BASE + offset + i) = word;
    }

    uint32_t status = ram_flash_wait();
    REG32(FLASH_PECR) &= ~FLASH_PECR_PROG;
    REG32(FLASH_PECR) &= ~FLASH_PECR_FPRG;
    REG32(SCB_ACTLR) = actlr;
    return status == 4u ? 0u : 2u;
}

static uint32_t RAMFUNC ram_verify_chunk(uint32_t offset, const uint8_t *chunk, uint8_t len) {
    for (uint8_t i = 0; i < len; ++i) {
        if (REG8(APP_FLASH_BASE + offset + i) != chunk[i]) {
            return 1u;
        }
    }
    return 0u;
}

static void RAMFUNC ram_reset_transfer(uint32_t *received, uint32_t *crc, uint8_t *first_count,
                                       uint32_t *next_page, uint32_t *next_half_offset,
                                       uint8_t *half_fill, uint32_t *erased_pages) {
    *received = 0;
    *crc = 0xFFFFFFFFu;
    *first_count = 0;
    *next_page = APP_FLASH_BASE;
    *next_half_offset = 0;
    *half_fill = 0;
    *erased_pages = 0;
}

static void RAMFUNC ram_stream_session(bool flash_mode, uint32_t initial_size, uint32_t initial_crc, uint32_t initial_data_baud) {
    char line[UPD_LINE_MAX];
    uint8_t chunk[UPD_STREAM_CHUNK_MAX];
    uint8_t half_page[UPD_FLASH_HALF_PAGE_SIZE];
    uint8_t first[16];
    uint8_t first_count = 0;
    uint32_t expected_size = initial_size;
    uint32_t expected_crc = initial_crc;
    uint32_t data_baud = initial_data_baud;
    uint32_t received = 0;
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t next_page = APP_FLASH_BASE;
    uint32_t next_half_offset = 0;
    uint8_t half_fill = 0;
    uint32_t erased_pages = 0;
    bool active = true;

    REG32(SYST_CSR) = 0;
    for (uint32_t i = 0; i < 8u; ++i) {
        REG32(NVIC_ICER0 + i * 4u) = 0xFFFFFFFFu;
        REG32(NVIC_ICPR0 + i * 4u) = 0xFFFFFFFFu;
    }
    if (flash_mode && ram_flash_unlock() != 0u) {
        ram_reply_err(CODE4('U', 'N', 'L', 'K'), 4);
        flash_mode = false;
    }

    ram_send_switch_baud(data_baud);
    ram_uart_set_baud(data_baud);
    ram_send_header(flash_mode, expected_size, expected_crc);

    while (1) {
        if (!ram_read_line(line, sizeof(line))) {
            ram_reply_err(CODE4('L', 'I', 'N', 'E'), 4);
            continue;
        }
        if (line[0] != 'U') {
            continue;
        }
        if (line[1] == 'C' && line[2] == 'L' && line[3] == 'K' && line[4] == '\0') {
            ram_send_clock_info(data_baud);
            continue;
        }
        if (line[1] == 'A' && line[2] == '\0') {
            ram_reset_transfer(&received, &crc, &first_count, &next_page, &next_half_offset, &half_fill, &erased_pages);
            active = false;
            ram_send_ok(CODE4('A', 'B', 'O', 'R'), 4);
            continue;
        }

        bool restart_cmd = false;
        bool restart_flash_mode = flash_mode;
        const char *restart_args = 0;
        if (line[1] == 'B') {
            restart_cmd = true;
            restart_args = &line[2];
        } else if (line[1] == 'F' && line[2] == 'B' && line[3] == 'E' && line[4] == 'G' &&
                   line[5] == 'I' && line[6] == 'N') {
            restart_cmd = true;
            restart_flash_mode = true;
            restart_args = &line[7];
        } else if (line[1] == 'S' && line[2] == 'I' && line[3] == 'M') {
            restart_cmd = true;
            restart_flash_mode = false;
            restart_args = &line[4];
        }
        if (restart_cmd) {
            const char *p = restart_args;
            while (*p == ' ') {
                ++p;
            }
            if (!ram_parse_hex32(&p, &expected_size)) {
                ram_reply_err(CODE4('B', 'E', 'G', 'I'), 4);
                continue;
            }
            while (*p == ' ') {
                ++p;
            }
            if (!ram_parse_hex32(&p, &expected_crc) || expected_size == 0u ||
                expected_size > UPD_APP_MAX_SIZE || (expected_size & 3u) != 0u) {
                ram_reply_err(CODE4('B', 'E', 'G', 'I'), 4);
                continue;
            }
            while (*p == ' ') {
                ++p;
            }
            uint32_t new_data_baud = data_baud;
            if (*p != '\0') {
                uint32_t parsed_baud = 0;
                if (!ram_parse_hex32(&p, &parsed_baud) ||
                    !(parsed_baud == 19200u || parsed_baud == 38400u ||
                      parsed_baud == 57600u || parsed_baud == 115200u ||
                      parsed_baud == 230400u || parsed_baud == 460800u ||
                      parsed_baud == 921600u)) {
                    ram_reply_err(CODE4('B', 'A', 'U', 'D'), 4);
                    continue;
                }
                new_data_baud = parsed_baud;
            }
            ram_reset_transfer(&received, &crc, &first_count, &next_page, &next_half_offset, &half_fill, &erased_pages);
            flash_mode = restart_flash_mode;
            if (flash_mode && ram_flash_unlock() != 0u) {
                ram_reply_err(CODE4('U', 'N', 'L', 'K'), 4);
                flash_mode = false;
                active = false;
                continue;
            }
            active = true;
            data_baud = new_data_baud;
            ram_send_switch_baud(data_baud);
            ram_uart_set_baud(data_baud);
            if (line[1] == 'B') {
                ram_send_ok(CODE4('R', 'E', 'S', 'T'), 4);
            } else {
                ram_send_header(flash_mode, expected_size, expected_crc);
            }
            continue;
        }

        if ((line[1] == 'E' && line[2] == '\0') || (line[1] == 'F' && line[2] == '\0')) {
            uint32_t final_crc = crc ^ 0xFFFFFFFFu;
            if (!active || received != expected_size) {
                ram_reply_err(CODE4('C', 'O', 'U', 'N'), 4);
                continue;
            }
            if (final_crc != expected_crc) {
                ram_reply_err(CODE4('C', 'R', 'C', ' '), 3);
                continue;
            }
            if (!ram_vector_plausible(first, first_count)) {
                ram_reply_err(CODE4('V', 'E', 'C', 'T'), 4);
                continue;
            }
            if (flash_mode) {
                if (half_fill != 0u) {
                    while (half_fill < UPD_FLASH_HALF_PAGE_SIZE) {
                        half_page[half_fill++] = 0xFFu;
                    }
                    if (ram_program_half_page(next_half_offset, half_page) != 0u ||
                        ram_verify_chunk(next_half_offset, half_page, UPD_FLASH_HALF_PAGE_SIZE) != 0u) {
                        ram_reply_err(CODE4('W', 'R', 'I', 'T'), 4);
                        active = false;
                        half_fill = 0;
                        continue;
                    }
                }
                ram_send_flash_ok(final_crc);
                while ((REG32(USART_SR(USART2_BASE)) & BIT(6)) == 0u) {
                }
                REG32(SCB_AIRCR) = 0x05FA0004u;
                while (1) {
                }
            }
            ram_send_sim_ok(erased_pages, expected_size, final_crc);
            continue;
        }

        if (line[1] != 'D') {
            ram_reply_err(CODE4('C', 'M', 'D', ' '), 3);
            continue;
        }
        if (!active) {
            ram_reply_err(CODE4('I', 'D', 'L', 'E'), 4);
            continue;
        }

        const char *p = &line[2];
        while (*p == ' ') {
            ++p;
        }
        uint8_t chunk_len = 0;
        bool ok = true;
        while (*p != '\0') {
            if (*p == ' ') {
                ++p;
                continue;
            }
            if (p[1] == '\0' || chunk_len >= UPD_STREAM_CHUNK_MAX) {
                ok = false;
                break;
            }
            chunk[chunk_len++] = (uint8_t)((ram_hex_value(p[0], &ok) << 4) | ram_hex_value(p[1], &ok));
            if (!ok) {
                break;
            }
            p += 2;
        }
        if (!ok || chunk_len == 0u || (chunk_len & 3u) != 0u ||
            received > expected_size || chunk_len > (expected_size - received)) {
            ram_reply_err(CODE4('D', 'A', 'T', 'A'), 4);
            continue;
        }
        for (uint8_t i = 0; i < chunk_len; ++i) {
            if (first_count < sizeof(first)) {
                first[first_count++] = chunk[i];
            }
            crc = ram_crc32_byte(crc, chunk[i]);
        }
        if (received == 0u && !ram_vector_plausible(first, first_count)) {
            ram_reply_err(CODE4('V', 'E', 'C', 'T'), 4);
            continue;
        }
        if (flash_mode) {
            uint32_t chunk_start = APP_FLASH_BASE + received;
            uint32_t chunk_end = chunk_start + chunk_len;
            while (next_page < chunk_end) {
                if (ram_erase_page(next_page) != 0u) {
                    ram_reply_err(CODE4('E', 'R', 'A', 'S'), 4);
                    active = false;
                    break;
                }
                next_page += UPD_FLASH_PAGE_SIZE;
                ++erased_pages;
            }
            if (!active) {
                continue;
            }
            for (uint8_t i = 0; i < chunk_len; ++i) {
                half_page[half_fill++] = chunk[i];
                if (half_fill == UPD_FLASH_HALF_PAGE_SIZE) {
                    if (ram_program_half_page(next_half_offset, half_page) != 0u ||
                        ram_verify_chunk(next_half_offset, half_page, UPD_FLASH_HALF_PAGE_SIZE) != 0u) {
                        ram_reply_err(CODE4('W', 'R', 'I', 'T'), 4);
                        active = false;
                        break;
                    }
                    next_half_offset += UPD_FLASH_HALF_PAGE_SIZE;
                    half_fill = 0;
                }
            }
            if (!active) {
                continue;
            }
        } else {
            uint32_t last = received + chunk_len;
            uint32_t page_count = (last + (UPD_FLASH_PAGE_SIZE - 1u)) / UPD_FLASH_PAGE_SIZE;
            if (page_count > erased_pages) {
                erased_pages = page_count;
            }
        }
        received += chunk_len;
        ram_send_ack(received);
    }
}
