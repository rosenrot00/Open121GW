#include "clock.h"

#include "hw.h"

static volatile uint32_t g_ms_ticks;
static uint32_t g_core_hz = 4194304u;

static uint8_t prescaler_shift(uint32_t field) {
    static const uint8_t shifts[16] = {
        0u, 0u, 0u, 0u, 1u, 2u, 3u, 4u,
        1u, 2u, 3u, 4u, 6u, 7u, 8u, 9u,
    };
    return shifts[field & 0xFu];
}

static bool wait_bits_set(uint32_t reg, uint32_t mask, uint32_t timeout) {
    while ((REG32(reg) & mask) != mask) {
        if (timeout-- == 0u) {
            return false;
        }
    }
    return true;
}

static bool wait_bits_clear(uint32_t reg, uint32_t mask, uint32_t timeout) {
    while ((REG32(reg) & mask) != 0u) {
        if (timeout-- == 0u) {
            return false;
        }
    }
    return true;
}

bool clock_init_stock_msi_4mhz(void) {
    REG32(SCB_VTOR) = APP_FLASH_BASE;

    REG32(RCC_CR) |= BIT(8);             /* MSI on, stock low-level init path. */
    if (!wait_bits_set(RCC_CR, BIT(9), 2000000u)) {
        return false;
    }

    REG32(RCC_CFGR) &= ~((3u << 0) | (0xFu << 4) | (7u << 8) | (7u << 11));
    (void)wait_bits_clear(RCC_CFGR, 0xCu, 2000000u);

    /*
     * Original EEVblog 2.05 does this through sub_801BF1A(0):
     * enable MSI, select SYSCLK=MSI, then set RCC_ICSCR MSIRANGE to 0xC000.
     * On STM32L1 that is range 6, 4.194304 MHz.
     */
    REG32(RCC_ICSCR) = (REG32(RCC_ICSCR) & ~0xE000u) | 0xC000u;
    REG32(RCC_CR) &= ~(BIT(24) | BIT(16)); /* ADC uses HSI, so keep it available as Stock does. */
    g_core_hz = 4194304u;
    return true;
}

void clock_init_systick_1ms(void) {
    uint32_t reload = (clock_core_hz() / 1000u) - 1u;
    REG32(SYST_RVR) = reload;
    REG32(SYST_CVR) = 0;
    REG32(SYST_CSR) = BIT(2) | BIT(1) | BIT(0);
}

void clock_systick_irq(void) {
    ++g_ms_ticks;
}

uint32_t clock_millis(void) {
    return g_ms_ticks;
}

uint32_t clock_core_hz(void) {
    return g_core_hz >> prescaler_shift((REG32(RCC_CFGR) >> 4) & 0xFu);
}

uint32_t clock_apb1_hz(void) {
    return clock_core_hz() >> prescaler_shift((REG32(RCC_CFGR) >> 8) & 0x7u);
}

void delay_ms(uint32_t ms) {
    uint32_t start = clock_millis();
    while ((uint32_t)(clock_millis() - start) < ms) {
        __asm__ volatile("wfi");
    }
}

void delay_cycles(uint32_t cycles) {
    while (cycles-- != 0u) {
        __asm__ volatile("nop");
    }
}
