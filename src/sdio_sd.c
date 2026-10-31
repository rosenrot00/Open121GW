#include "sdio_sd.h"

#include "clock.h"
#include "gpio.h"
#include "hw.h"

#define SDIO_BASE        0x40012C00u
#define SDIO_POWER       (SDIO_BASE + 0x00u)
#define SDIO_CLKCR       (SDIO_BASE + 0x04u)
#define SDIO_ARG         (SDIO_BASE + 0x08u)
#define SDIO_CMD         (SDIO_BASE + 0x0Cu)
#define SDIO_RESPCMD     (SDIO_BASE + 0x10u)
#define SDIO_RESP1       (SDIO_BASE + 0x14u)
#define SDIO_DTIMER      (SDIO_BASE + 0x24u)
#define SDIO_DLEN        (SDIO_BASE + 0x28u)
#define SDIO_DCTRL       (SDIO_BASE + 0x2Cu)
#define SDIO_STA         (SDIO_BASE + 0x34u)
#define SDIO_ICR         (SDIO_BASE + 0x38u)
#define SDIO_MASK        (SDIO_BASE + 0x3Cu)
#define SDIO_FIFO        (SDIO_BASE + 0x80u)
#define SDIO_CLKCR_CLKEN_BB 0x422580A0u
#define SDIO_DCTRL_DMAEN_BB 0x4225858Cu

#define DMA2_BASE        0x40026400u
#define DMA2_ISR         (DMA2_BASE + 0x00u)
#define DMA2_IFCR        (DMA2_BASE + 0x04u)
#define DMA2_CCR4        (DMA2_BASE + 0x44u)
#define DMA2_CNDTR4      (DMA2_BASE + 0x48u)
#define DMA2_CPAR4       (DMA2_BASE + 0x4Cu)
#define DMA2_CMAR4       (DMA2_BASE + 0x50u)
#define DMA2_CH4_FLAGS   0x0000F000u
#define DMA2_CH4_TCIF    BIT(13)
#define DMA2_CH4_TEIF    BIT(15)

#define SDIO_STA_CCRCFAIL BIT(0)
#define SDIO_STA_DCRCFAIL BIT(1)
#define SDIO_STA_CTIMEOUT BIT(2)
#define SDIO_STA_DTIMEOUT BIT(3)
#define SDIO_STA_TXUNDERR BIT(4)
#define SDIO_STA_RXOVERR  BIT(5)
#define SDIO_STA_CMDREND  BIT(6)
#define SDIO_STA_CMDSENT  BIT(7)
#define SDIO_STA_DATAEND  BIT(8)
#define SDIO_STA_STBITERR BIT(9)
#define SDIO_STA_DBCKEND  BIT(10)
#define SDIO_STA_CMDACT   BIT(11)
#define SDIO_STA_TXACT    BIT(12)
#define SDIO_STA_RXACT    BIT(13)
#define SDIO_STA_TXFIFOHE BIT(14)
#define SDIO_STA_RXDAVL   BIT(21)

#define SDIO_STATIC_FLAGS 0x000005FFu
#define SDIO_CMD_CPSMEN   BIT(10)
#define SDIO_CMD_WAITRESP_SHORT BIT(6)
#define SDIO_CMD_WAITRESP_LONG  (BIT(6) | BIT(7))
#define SDIO_DCTRL_DTEN   BIT(0)
#define SDIO_DCTRL_DTDIR  BIT(1)
#define SDIO_DCTRL_DMAEN  BIT(3)
#define SDIO_DCTRL_DBLOCK_512 (9u << 4)

#define DMA_CCR_EN        BIT(0)
#define DMA_CCR_DIR       BIT(4)
#define DMA_CCR_MINC      BIT(7)
#define DMA_CCR_PSIZE_32  BIT(9)
#define DMA_CCR_MSIZE_32  BIT(11)
#define DMA_CCR_PL_HIGH   BIT(13)

static bool g_sd_ready;
static bool g_sd_block_addressed;
static uint16_t g_sd_rca;
static uint8_t g_sd_debug_stage;
static bool g_sd_hse_session;
static uint32_t g_sd_debug_last_sta;
static uint32_t g_sd_debug_last_power;
static uint32_t g_sd_debug_last_clkcr;
static uint32_t g_sd_debug_last_rcc_cr;
static uint32_t g_sd_debug_last_rcc_cfgr;
static uint32_t g_sd_debug_data_count;
static uint32_t g_sd_debug_cmd_sta;
static uint32_t g_sd_debug_resp_cmd;
static uint32_t g_sd_debug_resp1;
static uint32_t g_sd_debug_dctrl;
static uint32_t g_sd_debug_dlen;
static uint32_t g_sd_debug_mask;
static uint32_t g_sd_debug_dma_ccr;
static uint32_t g_sd_debug_dma_cndtr;
static uint32_t g_sd_debug_dma_isr;
static uint8_t g_sd_debug_write_status_rc;
static uint32_t g_sd_debug_write_status;
static uint8_t g_sd_debug_verify_rc;
static uint32_t g_sd_debug_verify_offset;
static uint8_t g_sd_debug_verify_expected;
static uint8_t g_sd_debug_verify_actual;
static uint8_t g_sd_run_clk_div = 0x04u;
static uint8_t g_sd_write_mode = 2u;
static bool g_sd_hw_flow;
static uint32_t g_sd_dma_words[128];
static uint8_t g_sd_verify_block[512];

static sdio_sd_result_t sdio_cmd(uint8_t index, uint32_t arg, uint32_t waitresp, uint32_t *resp);

static void sdio_capture_debug_snapshot(void) {
    g_sd_debug_last_sta = REG32(SDIO_STA);
    g_sd_debug_last_power = REG32(SDIO_POWER);
    g_sd_debug_last_clkcr = REG32(SDIO_CLKCR);
    g_sd_debug_last_rcc_cr = REG32(RCC_CR);
    g_sd_debug_last_rcc_cfgr = REG32(RCC_CFGR);
    g_sd_debug_resp_cmd = REG32(SDIO_RESPCMD);
    g_sd_debug_resp1 = REG32(SDIO_RESP1);
    g_sd_debug_dctrl = REG32(SDIO_DCTRL);
    g_sd_debug_dlen = REG32(SDIO_DLEN);
    g_sd_debug_mask = REG32(SDIO_MASK);
    g_sd_debug_dma_ccr = REG32(DMA2_CCR4);
    g_sd_debug_dma_cndtr = REG32(DMA2_CNDTR4);
    g_sd_debug_dma_isr = REG32(DMA2_ISR);
}

static sdio_sd_result_t sdio_cmd_preserve_snapshot(uint8_t index, uint32_t arg, uint32_t *resp) {
    uint8_t saved_stage = g_sd_debug_stage;
    uint32_t saved_sta = g_sd_debug_last_sta;
    uint32_t saved_power = g_sd_debug_last_power;
    uint32_t saved_clkcr = g_sd_debug_last_clkcr;
    uint32_t saved_rcc_cr = g_sd_debug_last_rcc_cr;
    uint32_t saved_rcc_cfgr = g_sd_debug_last_rcc_cfgr;
    uint32_t saved_count = g_sd_debug_data_count;
    uint32_t saved_cmd_sta = g_sd_debug_cmd_sta;
    uint32_t saved_resp_cmd = g_sd_debug_resp_cmd;
    uint32_t saved_resp1 = g_sd_debug_resp1;
    uint32_t saved_dctrl = g_sd_debug_dctrl;
    uint32_t saved_dlen = g_sd_debug_dlen;
    uint32_t saved_mask = g_sd_debug_mask;
    uint32_t saved_dma_ccr = g_sd_debug_dma_ccr;
    uint32_t saved_dma_cndtr = g_sd_debug_dma_cndtr;
    uint32_t saved_dma_isr = g_sd_debug_dma_isr;

    sdio_sd_result_t rc = sdio_cmd(index, arg, SDIO_CMD_WAITRESP_SHORT, resp);

    g_sd_debug_stage = saved_stage;
    g_sd_debug_last_sta = saved_sta;
    g_sd_debug_last_power = saved_power;
    g_sd_debug_last_clkcr = saved_clkcr;
    g_sd_debug_last_rcc_cr = saved_rcc_cr;
    g_sd_debug_last_rcc_cfgr = saved_rcc_cfgr;
    g_sd_debug_data_count = saved_count;
    g_sd_debug_cmd_sta = saved_cmd_sta;
    g_sd_debug_resp_cmd = saved_resp_cmd;
    g_sd_debug_resp1 = saved_resp1;
    g_sd_debug_dctrl = saved_dctrl;
    g_sd_debug_dlen = saved_dlen;
    g_sd_debug_mask = saved_mask;
    g_sd_debug_dma_ccr = saved_dma_ccr;
    g_sd_debug_dma_cndtr = saved_dma_cndtr;
    g_sd_debug_dma_isr = saved_dma_isr;
    return rc;
}

static void sdio_capture_write_status(void) {
    uint32_t status = 0;
    sdio_sd_result_t rc = sdio_cmd_preserve_snapshot(13u, (uint32_t)g_sd_rca << 16, &status);
    g_sd_debug_write_status_rc = (uint8_t)rc;
    g_sd_debug_write_status = status;
}

static bool sdio_wait_card_transfer_ready(uint32_t loops) {
    uint32_t status = 0;
    while (loops-- != 0u) {
        sdio_sd_result_t rc = sdio_cmd_preserve_snapshot(13u, (uint32_t)g_sd_rca << 16, &status);
        g_sd_debug_write_status_rc = (uint8_t)rc;
        g_sd_debug_write_status = status;
        if (rc == SDIO_SD_OK && (status & 0x00000100u) != 0u &&
            ((status >> 9) & 0x0Fu) == 4u) {
            return true;
        }
        delay_ms(1u);
    }
    return false;
}

static bool sdio_block_matches(const uint8_t a[512], const uint8_t b[512]) {
    for (uint32_t i = 0; i < 512u; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static void sdio_clear_flags(void) {
    REG32(SDIO_ICR) = SDIO_STATIC_FLAGS;
}

static void sdio_clear_cmd_flags(void) {
    REG32(SDIO_ICR) = SDIO_STA_CCRCFAIL | SDIO_STA_CTIMEOUT |
                      SDIO_STA_CMDREND | SDIO_STA_CMDSENT;
}

static bool sdio_wait_not_busy(uint32_t mask) {
    uint32_t timeout = 200000u;
    while ((REG32(SDIO_STA) & mask) != 0u) {
        if (timeout-- == 0u) {
            return false;
        }
    }
    return true;
}

static void dma2_ch4_stop(void) {
    REG32(DMA2_CCR4) &= ~DMA_CCR_EN;
    REG32(DMA2_IFCR) = DMA2_CH4_FLAGS;
}

static void __attribute__((unused)) dma2_ch4_start_write(const uint32_t data_words[128]) {
    dma2_ch4_stop();
    REG32(DMA2_CPAR4) = SDIO_FIFO;
    REG32(DMA2_CMAR4) = (uint32_t)data_words;
    REG32(DMA2_CNDTR4) = 128u;
    REG32(DMA2_CCR4) = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_32 |
                       DMA_CCR_MSIZE_32 | DMA_CCR_PL_HIGH;
    REG32(DMA2_CCR4) |= DMA_CCR_EN;
}

static sdio_sd_result_t sdio_cmd(uint8_t index, uint32_t arg, uint32_t waitresp, uint32_t *resp) {
    if (!sdio_wait_not_busy(SDIO_STA_CMDACT)) {
        return SDIO_SD_ERR_TIMEOUT;
    }
    sdio_clear_flags();
    REG32(SDIO_ARG) = arg;
    REG32(SDIO_CMD) = SDIO_CMD_CPSMEN | waitresp | index;

    uint32_t want = waitresp == 0u ? SDIO_STA_CMDSENT : SDIO_STA_CMDREND;
    uint32_t timeout = 400000u;
    while ((REG32(SDIO_STA) & (want | SDIO_STA_CTIMEOUT | SDIO_STA_CCRCFAIL)) == 0u) {
        if (timeout-- == 0u) {
            sdio_capture_debug_snapshot();
            return SDIO_SD_ERR_TIMEOUT;
        }
    }

    uint32_t sta = REG32(SDIO_STA);
    g_sd_debug_cmd_sta = sta;
    sdio_capture_debug_snapshot();
    if ((sta & SDIO_STA_CTIMEOUT) != 0u) {
        if (index == 17u && (sta & (SDIO_STA_RXACT | SDIO_STA_RXDAVL)) != 0u) {
            sdio_clear_flags();
            return SDIO_SD_OK;
        }
        sdio_clear_flags();
        return SDIO_SD_ERR_TIMEOUT;
    }
    if ((sta & SDIO_STA_CCRCFAIL) != 0u && index != 0u && index != 8u && index != 41u) {
        sdio_clear_flags();
        return SDIO_SD_ERR_CMD;
    }
    if (resp != 0) {
        *resp = REG32(SDIO_RESP1);
    }
    if (index == 24u) {
        sdio_clear_cmd_flags();
    } else {
        sdio_clear_flags();
    }
    return SDIO_SD_OK;
}

static sdio_sd_result_t sdio_acmd(uint8_t index, uint32_t arg, uint32_t *resp) {
    sdio_sd_result_t rc = sdio_cmd(55u, (uint32_t)g_sd_rca << 16, SDIO_CMD_WAITRESP_SHORT, 0);
    if (rc != SDIO_SD_OK) {
        return rc;
    }
    return sdio_cmd(index, arg, SDIO_CMD_WAITRESP_SHORT, resp);
}

static void sdio_gpio_init(void) {
    gpio_config_pin(GPIOC_BASE, 8u, GPIO_MODE_AF, GPIO_PULL_UP, 12u);
    gpio_config_pin(GPIOC_BASE, 12u, GPIO_MODE_AF, GPIO_PULL_NONE, 12u);
    gpio_config_pin(GPIOD_BASE, 2u, GPIO_MODE_AF, GPIO_PULL_UP, 12u);
    REG32(GPIO_OSPEEDR(GPIOC_BASE)) |= (3u << (8u * 2u)) | (3u << (12u * 2u));
    REG32(GPIO_OSPEEDR(GPIOD_BASE)) |= (3u << (2u * 2u));
}

static void sdio_clock_slow(void) {
    REG32(SDIO_CLKCR) = 0x76u;
    REG32(SDIO_POWER) = 0x03u;
    REG32(SDIO_CLKCR_CLKEN_BB) = 1u;
    REG32(SDIO_CLKCR) = 0x76u | BIT(8);
}

static void sdio_clock_run(void) {
    uint32_t clkcr = (uint32_t)g_sd_run_clk_div | (g_sd_hw_flow ? BIT(14) : 0u);
    REG32(SDIO_CLKCR) = clkcr;
    REG32(SDIO_CLKCR_CLKEN_BB) = 1u;
    REG32(SDIO_CLKCR) = clkcr | BIT(8);
}

static bool wait_rcc_bits_set(uint32_t reg, uint32_t mask, uint32_t timeout) {
    while ((REG32(reg) & mask) != mask) {
        if (timeout-- == 0u) {
            return false;
        }
    }
    return true;
}

static bool wait_rcc_sysclk_status(uint32_t sws, uint32_t timeout) {
    while (((REG32(RCC_CFGR) >> 2) & 3u) != sws) {
        if (timeout-- == 0u) {
            return false;
        }
    }
    return true;
}

bool sdio_sd_begin_session(void) {
    if (g_sd_hse_session) {
        return true;
    }

    g_sd_debug_stage = 0xE1u;
    REG32(RCC_CR) |= BIT(16);
    if (!wait_rcc_bits_set(RCC_CR, BIT(17), 2000000u)) {
        return false;
    }
    REG32(RCC_CR) |= BIT(24);
    if (!wait_rcc_bits_set(RCC_CR, BIT(25), 2000000u)) {
        return false;
    }
    g_sd_debug_stage = 0xE2u;
    REG32(RCC_CFGR) = (REG32(RCC_CFGR) & ~3u) | 2u;
    if (!wait_rcc_sysclk_status(2u, 2000000u)) {
        return false;
    }
    g_sd_debug_stage = 0xE3u;
    g_sd_hse_session = true;
    g_sd_ready = false;
    return true;
}

void sdio_sd_end_session(void) {
    REG32(SDIO_POWER) = 0;
    REG32(SDIO_CLKCR_CLKEN_BB) = 0u;
    REG32(SDIO_CLKCR) = 0;
    REG32(RCC_APB2RSTR) |= BIT(11);
    REG32(RCC_APB2RSTR) &= ~BIT(11);
    REG32(RCC_APB2ENR) &= ~BIT(11);
    g_sd_ready = false;

    if (g_sd_hse_session) {
        REG32(RCC_CR) |= BIT(0);
        (void)wait_rcc_bits_set(RCC_CR, BIT(1), 2000000u);
        REG32(RCC_CFGR) &= ~3u;
        (void)wait_rcc_sysclk_status(0u, 2000000u);
        REG32(RCC_CR) &= ~BIT(24);
        REG32(RCC_CR) &= ~BIT(16);
        g_sd_hse_session = false;
    }
}

void sdio_sd_debug_clock_probe(void) {
    if (!sdio_sd_begin_session()) {
        sdio_capture_debug_snapshot();
        return;
    }
    gpio_enable_all_clocks();
    sdio_gpio_init();
    REG32(RCC_AHBENR) |= 0x02000000u;
    REG32(RCC_APB2ENR) |= BIT(11);
    (void)REG32(RCC_APB2ENR);
    REG32(RCC_APB2RSTR) |= BIT(11);
    REG32(RCC_APB2RSTR) &= ~BIT(11);
    sdio_clock_slow();
    delay_ms(2u);
    g_sd_debug_stage = 0xC1u;
    sdio_capture_debug_snapshot();
    sdio_sd_end_session();
}

void sdio_sd_debug_set_run_div(uint8_t div) {
    g_sd_run_clk_div = div;
    g_sd_ready = false;
    sdio_sd_end_session();
}

void sdio_sd_debug_set_hw_flow(bool enabled) {
    g_sd_hw_flow = enabled;
    g_sd_ready = false;
    sdio_sd_end_session();
}

void sdio_sd_debug_set_write_mode(uint8_t mode) {
    g_sd_write_mode = mode;
}

sdio_sd_result_t sdio_sd_debug_card_status(uint32_t *status) {
    sdio_sd_result_t rc = sdio_sd_init();
    if (rc != SDIO_SD_OK) {
        if (status != 0) {
            *status = 0;
        }
        return rc;
    }
    uint8_t saved_stage = g_sd_debug_stage;
    uint32_t saved_sta = g_sd_debug_last_sta;
    uint32_t saved_power = g_sd_debug_last_power;
    uint32_t saved_clkcr = g_sd_debug_last_clkcr;
    uint32_t saved_rcc_cr = g_sd_debug_last_rcc_cr;
    uint32_t saved_rcc_cfgr = g_sd_debug_last_rcc_cfgr;
    uint32_t saved_count = g_sd_debug_data_count;
    uint32_t saved_cmd_sta = g_sd_debug_cmd_sta;
    uint32_t saved_resp_cmd = g_sd_debug_resp_cmd;
    uint32_t saved_resp1 = g_sd_debug_resp1;
    uint32_t saved_dctrl = g_sd_debug_dctrl;
    uint32_t saved_dlen = g_sd_debug_dlen;
    uint32_t saved_mask = g_sd_debug_mask;
    uint32_t saved_dma_ccr = g_sd_debug_dma_ccr;
    uint32_t saved_dma_cndtr = g_sd_debug_dma_cndtr;
    uint32_t saved_dma_isr = g_sd_debug_dma_isr;

    rc = sdio_cmd(13u, (uint32_t)g_sd_rca << 16, SDIO_CMD_WAITRESP_SHORT, status);

    g_sd_debug_stage = saved_stage;
    g_sd_debug_last_sta = saved_sta;
    g_sd_debug_last_power = saved_power;
    g_sd_debug_last_clkcr = saved_clkcr;
    g_sd_debug_last_rcc_cr = saved_rcc_cr;
    g_sd_debug_last_rcc_cfgr = saved_rcc_cfgr;
    g_sd_debug_data_count = saved_count;
    g_sd_debug_cmd_sta = saved_cmd_sta;
    g_sd_debug_resp_cmd = saved_resp_cmd;
    g_sd_debug_resp1 = saved_resp1;
    g_sd_debug_dctrl = saved_dctrl;
    g_sd_debug_dlen = saved_dlen;
    g_sd_debug_mask = saved_mask;
    g_sd_debug_dma_ccr = saved_dma_ccr;
    g_sd_debug_dma_cndtr = saved_dma_cndtr;
    g_sd_debug_dma_isr = saved_dma_isr;
    return rc;
}

sdio_sd_result_t sdio_sd_init(void) {
    if (g_sd_ready) {
        return SDIO_SD_OK;
    }

    g_sd_debug_stage = 1u;
    gpio_enable_all_clocks();
    sdio_gpio_init();
    REG32(RCC_AHBENR) |= 0x02000000u;
    REG32(RCC_APB2ENR) |= BIT(11);
    (void)REG32(RCC_APB2ENR);
    REG32(RCC_APB2RSTR) |= BIT(11);
    REG32(RCC_APB2RSTR) &= ~BIT(11);
    sdio_clock_slow();
    delay_ms(2u);

    g_sd_rca = 0;
    g_sd_block_addressed = false;
    g_sd_debug_stage = 2u;
    sdio_sd_result_t rc = sdio_cmd(0u, 0u, 0u, 0);
    if (rc != SDIO_SD_OK) {
        return rc;
    }

    uint32_t resp = 0;
    g_sd_debug_stage = 3u;
    (void)sdio_cmd(8u, 0x1AAu, SDIO_CMD_WAITRESP_SHORT, &resp);

    uint32_t ocr = 0;
    g_sd_debug_stage = 4u;
    for (uint32_t i = 0; i < 60000u; ++i) {
        rc = sdio_acmd(41u, 0x40100000u, &ocr);
        if (rc != SDIO_SD_OK) {
            return rc;
        }
        if ((ocr & 0x80000000u) != 0u) {
            break;
        }
    }
    if ((ocr & 0x80000000u) == 0u) {
        return SDIO_SD_ERR_TIMEOUT;
    }
    g_sd_block_addressed = (ocr & 0x40000000u) != 0u;

    g_sd_debug_stage = 5u;
    rc = sdio_cmd(2u, 0u, SDIO_CMD_WAITRESP_LONG, 0);
    if (rc != SDIO_SD_OK) {
        return rc;
    }
    g_sd_debug_stage = 6u;
    rc = sdio_cmd(3u, 0u, SDIO_CMD_WAITRESP_SHORT, &resp);
    if (rc != SDIO_SD_OK) {
        return rc;
    }
    g_sd_rca = (uint16_t)(resp >> 16);

    g_sd_debug_stage = 7u;
    rc = sdio_cmd(7u, (uint32_t)g_sd_rca << 16, SDIO_CMD_WAITRESP_SHORT, 0);
    if (rc != SDIO_SD_OK) {
        return rc;
    }
    if (!g_sd_block_addressed) {
        g_sd_debug_stage = 8u;
        rc = sdio_cmd(16u, 512u, SDIO_CMD_WAITRESP_SHORT, 0);
        if (rc != SDIO_SD_OK) {
            return rc;
        }
    }

    sdio_clock_run();
    g_sd_ready = true;
    g_sd_debug_stage = 0xFFu;
    return SDIO_SD_OK;
}

bool sdio_sd_ready(void) {
    return g_sd_ready;
}

sdio_sd_result_t sdio_sd_read_block(uint32_t lba, uint8_t out[512]) {
    sdio_sd_result_t rc = sdio_sd_init();
    if (rc != SDIO_SD_OK) {
        return rc;
    }

    uint32_t addr = g_sd_block_addressed ? lba : (lba << 9);
    sdio_clear_flags();
    g_sd_debug_stage = 0x11u;
    g_sd_debug_data_count = 0;
    REG32(SDIO_DTIMER) = 0xFFFFFFFFu;
    REG32(SDIO_DLEN) = 512u;
    REG32(SDIO_DCTRL) = SDIO_DCTRL_DTEN | SDIO_DCTRL_DTDIR | SDIO_DCTRL_DBLOCK_512;
    rc = sdio_cmd(17u, addr, SDIO_CMD_WAITRESP_SHORT, 0);
    if (rc != SDIO_SD_OK) {
        REG32(SDIO_DCTRL) = 0;
        return rc;
    }

    uint32_t count = 0;
    uint32_t idle_timeout = 8000000u;
    while (true) {
        while ((REG32(SDIO_STA) & SDIO_STA_RXDAVL) != 0u && count < 512u) {
            uint32_t word = REG32(SDIO_FIFO);
            out[count++] = (uint8_t)word;
            out[count++] = (uint8_t)(word >> 8);
            out[count++] = (uint8_t)(word >> 16);
            out[count++] = (uint8_t)(word >> 24);
            idle_timeout = 8000000u;
        }

        uint32_t sta_now = REG32(SDIO_STA);
        if ((sta_now & (SDIO_STA_DATAEND | SDIO_STA_DTIMEOUT | SDIO_STA_DCRCFAIL |
                        SDIO_STA_RXOVERR | SDIO_STA_STBITERR)) != 0u) {
            break;
        }
        if (idle_timeout-- == 0u) {
            REG32(SDIO_DCTRL) = 0;
            g_sd_debug_data_count = count;
            sdio_capture_debug_snapshot();
            sdio_clear_flags();
            return SDIO_SD_ERR_TIMEOUT;
        }
    }
    while ((REG32(SDIO_STA) & SDIO_STA_RXDAVL) != 0u && count < 512u) {
        uint32_t word = REG32(SDIO_FIFO);
        out[count++] = (uint8_t)word;
        out[count++] = (uint8_t)(word >> 8);
        out[count++] = (uint8_t)(word >> 16);
        out[count++] = (uint8_t)(word >> 24);
    }
    uint32_t sta = REG32(SDIO_STA);
    REG32(SDIO_DCTRL) = 0;
    g_sd_debug_data_count = count;
    sdio_capture_debug_snapshot();
    sdio_clear_flags();
    return ((sta & (SDIO_STA_DTIMEOUT | SDIO_STA_DCRCFAIL | SDIO_STA_RXOVERR | SDIO_STA_STBITERR)) != 0u ||
            count != 512u) ? SDIO_SD_ERR_DATA : SDIO_SD_OK;
}

static bool sdio_verify_written_block(uint32_t lba, const uint8_t data[512]) {
    uint8_t saved_stage = g_sd_debug_stage;
    uint32_t saved_sta = g_sd_debug_last_sta;
    uint32_t saved_power = g_sd_debug_last_power;
    uint32_t saved_clkcr = g_sd_debug_last_clkcr;
    uint32_t saved_rcc_cr = g_sd_debug_last_rcc_cr;
    uint32_t saved_rcc_cfgr = g_sd_debug_last_rcc_cfgr;
    uint32_t saved_count = g_sd_debug_data_count;
    uint32_t saved_cmd_sta = g_sd_debug_cmd_sta;
    uint32_t saved_resp_cmd = g_sd_debug_resp_cmd;
    uint32_t saved_resp1 = g_sd_debug_resp1;
    uint32_t saved_dctrl = g_sd_debug_dctrl;
    uint32_t saved_dlen = g_sd_debug_dlen;
    uint32_t saved_mask = g_sd_debug_mask;
    uint32_t saved_dma_ccr = g_sd_debug_dma_ccr;
    uint32_t saved_dma_cndtr = g_sd_debug_dma_cndtr;
    uint32_t saved_dma_isr = g_sd_debug_dma_isr;

    sdio_sd_result_t rc = sdio_sd_read_block(lba, g_sd_verify_block);
    g_sd_debug_verify_rc = (uint8_t)rc;
    bool ok = rc == SDIO_SD_OK && sdio_block_matches(g_sd_verify_block, data);
    if (rc == SDIO_SD_OK && !ok) {
        for (uint32_t i = 0; i < 512u; ++i) {
            if (g_sd_verify_block[i] != data[i]) {
                g_sd_debug_verify_offset = i;
                g_sd_debug_verify_expected = data[i];
                g_sd_debug_verify_actual = g_sd_verify_block[i];
                break;
            }
        }
    }

    g_sd_debug_stage = saved_stage;
    g_sd_debug_last_sta = saved_sta;
    g_sd_debug_last_power = saved_power;
    g_sd_debug_last_clkcr = saved_clkcr;
    g_sd_debug_last_rcc_cr = saved_rcc_cr;
    g_sd_debug_last_rcc_cfgr = saved_rcc_cfgr;
    g_sd_debug_data_count = saved_count;
    g_sd_debug_cmd_sta = saved_cmd_sta;
    g_sd_debug_resp_cmd = saved_resp_cmd;
    g_sd_debug_resp1 = saved_resp1;
    g_sd_debug_dctrl = saved_dctrl;
    g_sd_debug_dlen = saved_dlen;
    g_sd_debug_mask = saved_mask;
    g_sd_debug_dma_ccr = saved_dma_ccr;
    g_sd_debug_dma_cndtr = saved_dma_cndtr;
    g_sd_debug_dma_isr = saved_dma_isr;
    return ok;
}

sdio_sd_result_t sdio_sd_write_block(uint32_t lba, const uint8_t data[512]) {
    sdio_sd_result_t rc = sdio_sd_init();
    if (rc != SDIO_SD_OK) {
        return rc;
    }

    uint32_t addr = g_sd_block_addressed ? lba : (lba << 9);
    g_sd_debug_write_status_rc = 0xFFu;
    g_sd_debug_write_status = 0;
    g_sd_debug_verify_rc = 0xFFu;
    g_sd_debug_verify_offset = 0xFFFFFFFFu;
    g_sd_debug_verify_expected = 0;
    g_sd_debug_verify_actual = 0;
    for (uint32_t i = 0; i < 128u; ++i) {
        uint32_t off = i * 4u;
        g_sd_dma_words[i] = (uint32_t)data[off] |
                            ((uint32_t)data[off + 1u] << 8) |
                            ((uint32_t)data[off + 2u] << 16) |
                            ((uint32_t)data[off + 3u] << 24);
    }

    g_sd_debug_stage = 0x18u;
    g_sd_debug_data_count = 0;
    sdio_clock_run();
    REG32(SDIO_DCTRL) = 0;

    if (!g_sd_block_addressed) {
        rc = sdio_cmd(16u, 512u, SDIO_CMD_WAITRESP_SHORT, 0);
        if (rc != SDIO_SD_OK) {
            return rc;
        }
    }

    if (g_sd_write_mode == 2u) {
        g_sd_debug_stage = 0x19u;
        rc = sdio_cmd(16u, 512u, SDIO_CMD_WAITRESP_SHORT, 0);
        if (rc != SDIO_SD_OK) {
            return rc;
        }
        rc = sdio_acmd(23u, 1u, 0);
        if (rc != SDIO_SD_OK) {
            return rc;
        }
        g_sd_debug_stage = 0x1Au;
        rc = sdio_cmd(25u, addr, SDIO_CMD_WAITRESP_SHORT, 0);
        if (rc != SDIO_SD_OK) {
            return rc;
        }

        sdio_clear_flags();
        REG32(SDIO_DTIMER) = 0xFFFFFFFFu;
        REG32(SDIO_DLEN) = 512u;
        REG32(SDIO_MASK) = 0x32Au;
        dma2_ch4_start_write(g_sd_dma_words);
        REG32(SDIO_DCTRL) = SDIO_DCTRL_DTEN | SDIO_DCTRL_DBLOCK_512 | SDIO_DCTRL_DMAEN;

        g_sd_debug_stage = 0x1Cu;
        uint32_t timeout = 1000000u;
        while ((REG32(SDIO_STA) & (SDIO_STA_DATAEND | SDIO_STA_DTIMEOUT | SDIO_STA_DCRCFAIL |
                                   SDIO_STA_TXUNDERR | SDIO_STA_STBITERR)) == 0u) {
            if ((REG32(DMA2_ISR) & DMA2_CH4_TEIF) != 0u) {
                break;
            }
            if (timeout-- == 0u) {
                REG32(SDIO_DCTRL) = 0;
                g_sd_debug_data_count = 512u - REG32(DMA2_CNDTR4) * 4u;
                sdio_capture_debug_snapshot();
                dma2_ch4_stop();
                sdio_capture_write_status();
                sdio_clear_flags();
                return SDIO_SD_ERR_TIMEOUT;
            }
        }

        uint32_t sta = REG32(SDIO_STA);
        g_sd_debug_data_count = 512u - REG32(DMA2_CNDTR4) * 4u;
        sdio_capture_debug_snapshot();
        REG32(SDIO_DCTRL) = 0;
        dma2_ch4_stop();
        (void)sdio_cmd_preserve_snapshot(12u, 0u, 0);
        (void)sdio_wait_card_transfer_ready(250u);

        uint32_t error_mask = SDIO_STA_DTIMEOUT | SDIO_STA_DCRCFAIL | SDIO_STA_TXUNDERR | SDIO_STA_STBITERR;
        bool write_error = (sta & error_mask) != 0u ||
                           (REG32(DMA2_ISR) & DMA2_CH4_TEIF) != 0u ||
                           g_sd_debug_data_count != 512u;
        if (write_error) {
            sdio_capture_write_status();
        }
        sdio_clear_flags();
        if (write_error) {
            bool crc_only = (sta & error_mask) == SDIO_STA_DCRCFAIL &&
                            (REG32(DMA2_ISR) & DMA2_CH4_TEIF) == 0u &&
                            g_sd_debug_data_count == 512u &&
                            g_sd_debug_write_status_rc == SDIO_SD_OK &&
                            g_sd_debug_write_status == 0x00000900u;
            if (crc_only && sdio_verify_written_block(lba, data)) {
                return SDIO_SD_OK;
            }
            return SDIO_SD_ERR_DATA;
        }
        return sdio_verify_written_block(lba, data) ? SDIO_SD_OK : SDIO_SD_ERR_DATA;
    }

    if (g_sd_write_mode == 0u) {
        sdio_clear_flags();
        REG32(SDIO_DTIMER) = 0xFFFFFFFFu;
        REG32(SDIO_DLEN) = 512u;
        REG32(SDIO_MASK) = 0x32Au;
        dma2_ch4_start_write(g_sd_dma_words);
        REG32(SDIO_DCTRL) = SDIO_DCTRL_DTEN | SDIO_DCTRL_DBLOCK_512 | SDIO_DCTRL_DMAEN;

        g_sd_debug_stage = 0x1Au;
        rc = sdio_cmd(24u, addr, SDIO_CMD_WAITRESP_SHORT, 0);
        if (rc != SDIO_SD_OK) {
            REG32(SDIO_DCTRL) = 0;
            dma2_ch4_stop();
            return rc;
        }

        g_sd_debug_stage = 0x1Bu;
        uint32_t timeout = 1000000u;
        while ((REG32(SDIO_STA) & (SDIO_STA_DATAEND | SDIO_STA_DTIMEOUT | SDIO_STA_DCRCFAIL |
                                   SDIO_STA_TXUNDERR | SDIO_STA_STBITERR)) == 0u) {
            if ((REG32(DMA2_ISR) & DMA2_CH4_TEIF) != 0u) {
                break;
            }
            if (timeout-- == 0u) {
                REG32(SDIO_DCTRL) = 0;
                g_sd_debug_data_count = 512u - REG32(DMA2_CNDTR4) * 4u;
                sdio_capture_debug_snapshot();
                dma2_ch4_stop();
                sdio_capture_write_status();
                sdio_clear_flags();
                return SDIO_SD_ERR_TIMEOUT;
            }
        }

        uint32_t sta = REG32(SDIO_STA);
        g_sd_debug_data_count = 512u - REG32(DMA2_CNDTR4) * 4u;
        sdio_capture_debug_snapshot();
        REG32(SDIO_DCTRL) = 0;
        dma2_ch4_stop();
        uint32_t error_mask = SDIO_STA_DTIMEOUT | SDIO_STA_DCRCFAIL | SDIO_STA_TXUNDERR | SDIO_STA_STBITERR;
        bool write_error = (sta & error_mask) != 0u ||
                           (REG32(DMA2_ISR) & DMA2_CH4_TEIF) != 0u ||
                           g_sd_debug_data_count != 512u;
        if (write_error) {
            sdio_capture_write_status();
        }
        sdio_clear_flags();
        if (write_error) {
            bool crc_only = (sta & error_mask) == SDIO_STA_DCRCFAIL &&
                            (REG32(DMA2_ISR) & DMA2_CH4_TEIF) == 0u &&
                            g_sd_debug_data_count == 512u &&
                            g_sd_debug_write_status_rc == SDIO_SD_OK &&
                            g_sd_debug_write_status == 0x00000900u;
            if (crc_only && sdio_verify_written_block(lba, data)) {
                return SDIO_SD_OK;
            }
            return SDIO_SD_ERR_DATA;
        }
        return SDIO_SD_OK;
    }

    g_sd_debug_stage = 0x1Au;
    sdio_clear_flags();
    rc = sdio_cmd(24u, addr, SDIO_CMD_WAITRESP_SHORT, 0);
    if (rc != SDIO_SD_OK) {
        REG32(SDIO_DCTRL) = 0;
        dma2_ch4_stop();
        return rc;
    }

    sdio_clear_flags();
    REG32(SDIO_DTIMER) = 0xFFFFFFFFu;
    REG32(SDIO_DLEN) = 512u;
    REG32(SDIO_MASK) = 0x32Au;
    dma2_ch4_stop();
    REG32(SDIO_DCTRL) = SDIO_DCTRL_DTEN | SDIO_DCTRL_DBLOCK_512;

    g_sd_debug_stage = 0x1Bu;
    uint32_t count = 0;
    uint32_t timeout = 1000000u;
    while ((REG32(SDIO_STA) & (SDIO_STA_DATAEND | SDIO_STA_DTIMEOUT | SDIO_STA_DCRCFAIL |
                               SDIO_STA_TXUNDERR | SDIO_STA_STBITERR)) == 0u) {
        while ((REG32(SDIO_STA) & SDIO_STA_TXFIFOHE) != 0u && count < 512u) {
            uint32_t word = (uint32_t)data[count] |
                            ((uint32_t)data[count + 1u] << 8) |
                            ((uint32_t)data[count + 2u] << 16) |
                            ((uint32_t)data[count + 3u] << 24);
            REG32(SDIO_FIFO) = word;
            count += 4u;
        }
        if (timeout-- == 0u) {
            REG32(SDIO_DCTRL) = 0;
            g_sd_debug_data_count = count;
            sdio_capture_debug_snapshot();
            sdio_capture_write_status();
            sdio_clear_flags();
            return SDIO_SD_ERR_TIMEOUT;
        }
    }

    uint32_t sta = REG32(SDIO_STA);
    g_sd_debug_data_count = count;
    sdio_capture_debug_snapshot();
    REG32(SDIO_DCTRL) = 0;
    uint32_t error_mask = SDIO_STA_DTIMEOUT | SDIO_STA_DCRCFAIL | SDIO_STA_TXUNDERR | SDIO_STA_STBITERR;
    bool write_error = (sta & error_mask) != 0u || g_sd_debug_data_count != 512u;
    if (write_error) {
        sdio_capture_write_status();
    }
    sdio_clear_flags();
    if (write_error) {
        bool crc_only = (sta & error_mask) == SDIO_STA_DCRCFAIL &&
                        g_sd_debug_data_count == 512u &&
                        g_sd_debug_write_status_rc == SDIO_SD_OK &&
                        g_sd_debug_write_status == 0x00000900u;
        if (crc_only && sdio_verify_written_block(lba, data)) {
            return SDIO_SD_OK;
        }
        return SDIO_SD_ERR_DATA;
    }
    return SDIO_SD_OK;
}

uint32_t sdio_sd_debug_power(void) {
    return g_sd_debug_last_power;
}

uint32_t sdio_sd_debug_clkcr(void) {
    return g_sd_debug_last_clkcr;
}

uint32_t sdio_sd_debug_sta(void) {
    return g_sd_debug_last_sta;
}

uint8_t sdio_sd_debug_stage(void) {
    return g_sd_debug_stage;
}

uint32_t sdio_sd_debug_rcc_cr(void) {
    return g_sd_debug_last_rcc_cr != 0u ? g_sd_debug_last_rcc_cr : REG32(RCC_CR);
}

uint32_t sdio_sd_debug_rcc_cfgr(void) {
    return g_sd_debug_last_rcc_cfgr != 0u ? g_sd_debug_last_rcc_cfgr : REG32(RCC_CFGR);
}

uint32_t sdio_sd_debug_data_count(void) {
    return g_sd_debug_data_count;
}

uint32_t sdio_sd_debug_cmd_sta(void) {
    return g_sd_debug_cmd_sta;
}

uint32_t sdio_sd_debug_resp_cmd(void) {
    return g_sd_debug_resp_cmd;
}

uint32_t sdio_sd_debug_resp1(void) {
    return g_sd_debug_resp1;
}

uint32_t sdio_sd_debug_dctrl(void) {
    return g_sd_debug_dctrl;
}

uint32_t sdio_sd_debug_dlen(void) {
    return g_sd_debug_dlen;
}

uint32_t sdio_sd_debug_mask(void) {
    return g_sd_debug_mask;
}

uint32_t sdio_sd_debug_dma_ccr(void) {
    return g_sd_debug_dma_ccr;
}

uint32_t sdio_sd_debug_dma_cndtr(void) {
    return g_sd_debug_dma_cndtr;
}

uint32_t sdio_sd_debug_dma_isr(void) {
    return g_sd_debug_dma_isr;
}

uint8_t sdio_sd_debug_write_status_rc(void) {
    return g_sd_debug_write_status_rc;
}

uint32_t sdio_sd_debug_write_status(void) {
    return g_sd_debug_write_status;
}

uint8_t sdio_sd_debug_verify_rc(void) {
    return g_sd_debug_verify_rc;
}

uint32_t sdio_sd_debug_verify_offset(void) {
    return g_sd_debug_verify_offset;
}

uint8_t sdio_sd_debug_verify_expected(void) {
    return g_sd_debug_verify_expected;
}

uint8_t sdio_sd_debug_verify_actual(void) {
    return g_sd_debug_verify_actual;
}

uint16_t sdio_sd_debug_rca(void) {
    return g_sd_rca;
}

bool sdio_sd_debug_block_addressed(void) {
    return g_sd_block_addressed;
}

uint8_t sdio_sd_debug_write_mode(void) {
    return g_sd_write_mode;
}

bool sdio_sd_debug_hw_flow(void) {
    return g_sd_hw_flow;
}
