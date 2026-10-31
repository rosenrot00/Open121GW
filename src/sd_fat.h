#ifndef EEVBLOG_SD_FAT_H
#define EEVBLOG_SD_FAT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SD_FAT_OK = 0,
    SD_FAT_ERR_CARD = 1,
    SD_FAT_ERR_FORMAT = 2,
    SD_FAT_ERR_FULL = 3,
    SD_FAT_ERR_IO = 4,
} sd_fat_result_t;

sd_fat_result_t sd_fat_append_eevlog(const char *line);
sd_fat_result_t sd_fat_append_eevlog_with_header(const char *header, const char *line);
sd_fat_result_t sd_fat_append_named_83_with_header(const char name83[11],
                                                   const char *header,
                                                   const char *line);
sd_fat_result_t sd_fat_file_exists_83(const char name83[11], bool *exists);
sd_fat_result_t sd_fat_read_named_83(const char name83[11],
                                     uint32_t offset,
                                     uint8_t *out,
                                     uint32_t max_len,
                                     uint32_t *out_len,
                                     uint32_t *out_size);
uint8_t sd_fat_last_card_error(void);

#endif
