#ifndef EEVBLOG_SDIO_SD_H
#define EEVBLOG_SDIO_SD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SDIO_SD_OK = 0,
    SDIO_SD_ERR_TIMEOUT = 1,
    SDIO_SD_ERR_CMD = 2,
    SDIO_SD_ERR_DATA = 3,
    SDIO_SD_ERR_UNSUPPORTED = 4,
} sdio_sd_result_t;

sdio_sd_result_t sdio_sd_init(void);
sdio_sd_result_t sdio_sd_read_block(uint32_t lba, uint8_t out[512]);
sdio_sd_result_t sdio_sd_write_block(uint32_t lba, const uint8_t data[512]);
bool sdio_sd_ready(void);
bool sdio_sd_begin_session(void);
void sdio_sd_end_session(void);
void sdio_sd_debug_clock_probe(void);
void sdio_sd_debug_set_run_div(uint8_t div);
void sdio_sd_debug_set_hw_flow(bool enabled);
void sdio_sd_debug_set_write_mode(uint8_t mode);
sdio_sd_result_t sdio_sd_debug_card_status(uint32_t *status);
uint32_t sdio_sd_debug_power(void);
uint32_t sdio_sd_debug_clkcr(void);
uint32_t sdio_sd_debug_sta(void);
uint32_t sdio_sd_debug_rcc_cr(void);
uint32_t sdio_sd_debug_rcc_cfgr(void);
uint32_t sdio_sd_debug_data_count(void);
uint8_t sdio_sd_debug_stage(void);
uint32_t sdio_sd_debug_cmd_sta(void);
uint32_t sdio_sd_debug_resp_cmd(void);
uint32_t sdio_sd_debug_resp1(void);
uint32_t sdio_sd_debug_dctrl(void);
uint32_t sdio_sd_debug_dlen(void);
uint32_t sdio_sd_debug_mask(void);
uint32_t sdio_sd_debug_dma_ccr(void);
uint32_t sdio_sd_debug_dma_cndtr(void);
uint32_t sdio_sd_debug_dma_isr(void);
uint8_t sdio_sd_debug_write_status_rc(void);
uint32_t sdio_sd_debug_write_status(void);
uint8_t sdio_sd_debug_verify_rc(void);
uint32_t sdio_sd_debug_verify_offset(void);
uint8_t sdio_sd_debug_verify_expected(void);
uint8_t sdio_sd_debug_verify_actual(void);
uint16_t sdio_sd_debug_rca(void);
bool sdio_sd_debug_block_addressed(void);
uint8_t sdio_sd_debug_write_mode(void);
bool sdio_sd_debug_hw_flow(void);

#endif
