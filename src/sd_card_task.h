#ifndef EEVBLOG_SD_CARD_TASK_H
#define EEVBLOG_SD_CARD_TASK_H

#include <stdbool.h>
#include <stdint.h>

#include "sd_fat.h"

void sd_card_task_init(void);
void sd_card_task_poll(uint32_t now_ms);
void sd_card_task_notify_measurement_boundary(void);
bool sd_card_task_append_named_83_with_header(const char name83[11],
                                              const char *header,
                                              const char *line);
bool sd_card_task_busy(void);
uint8_t sd_card_task_queue_depth(void);
bool sd_card_task_error_pending(void);
sd_fat_result_t sd_card_task_last_result(void);
void sd_card_task_clear_error(void);

#endif
