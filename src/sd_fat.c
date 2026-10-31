#include "sd_fat.h"

#include "sdio_sd.h"

typedef struct {
    uint32_t part_lba;
    uint32_t fat_lba;
    uint32_t data_lba;
    uint32_t root_cluster;
    uint32_t fat_sectors;
    uint32_t total_clusters;
    uint16_t reserved;
    uint8_t sectors_per_cluster;
    uint8_t fat_count;
} fat32_volume_t;

static uint8_t g_sector[512];
static uint8_t g_last_card_error;
static fat32_volume_t g_vol;
static bool g_mounted;

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t c_strlen(const char *s) {
    uint32_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static bool c_memeq(const uint8_t *a, const char *b, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        if (a[i] != (uint8_t)b[i]) {
            return false;
        }
    }
    return true;
}

static sd_fat_result_t card_read(uint32_t lba, uint8_t out[512]) {
    sdio_sd_result_t rc = sdio_sd_read_block(lba, out);
    g_last_card_error = (uint8_t)rc;
    return rc == SDIO_SD_OK ? SD_FAT_OK : SD_FAT_ERR_CARD;
}

static sd_fat_result_t card_write(uint32_t lba, const uint8_t data[512]) {
    sdio_sd_result_t rc = sdio_sd_write_block(lba, data);
    g_last_card_error = (uint8_t)rc;
    return rc == SDIO_SD_OK ? SD_FAT_OK : SD_FAT_ERR_CARD;
}

static void close_card_session(void) {
    sdio_sd_end_session();
    g_mounted = false;
}

static uint32_t cluster_lba(uint32_t cluster) {
    return g_vol.data_lba + (cluster - 2u) * g_vol.sectors_per_cluster;
}

static sd_fat_result_t mount_fat32(void) {
    if (g_mounted) {
        return SD_FAT_OK;
    }

    sd_fat_result_t rc = card_read(0u, g_sector);
    if (rc != SD_FAT_OK) {
        return rc;
    }
    if (g_sector[510] != 0x55u || g_sector[511] != 0xAAu) {
        return SD_FAT_ERR_FORMAT;
    }

    uint32_t part = 0u;
    if (g_sector[0] != 0xEBu && g_sector[0] != 0xE9u) {
        uint8_t type = g_sector[0x1BEu + 4u];
        if (type == 0x0Bu || type == 0x0Cu || type == 0x1Bu || type == 0x1Cu) {
            part = rd32(&g_sector[0x1BEu + 8u]);
        } else {
            return SD_FAT_ERR_FORMAT;
        }
    }

    rc = card_read(part, g_sector);
    if (rc != SD_FAT_OK) {
        return rc;
    }
    if (rd16(&g_sector[11]) != 512u || g_sector[510] != 0x55u || g_sector[511] != 0xAAu) {
        return SD_FAT_ERR_FORMAT;
    }
    uint8_t spc = g_sector[13];
    uint16_t reserved = rd16(&g_sector[14]);
    uint8_t fats = g_sector[16];
    uint16_t root_entries = rd16(&g_sector[17]);
    uint16_t fatsz16 = rd16(&g_sector[22]);
    uint32_t total = rd16(&g_sector[19]);
    if (total == 0u) {
        total = rd32(&g_sector[32]);
    }
    uint32_t fatsz = rd32(&g_sector[36]);
    uint32_t root_cluster = rd32(&g_sector[44]);
    if (spc == 0u || reserved == 0u || fats == 0u || root_entries != 0u ||
        fatsz16 != 0u || fatsz == 0u || root_cluster < 2u) {
        return SD_FAT_ERR_FORMAT;
    }

    g_vol.part_lba = part;
    g_vol.reserved = reserved;
    g_vol.sectors_per_cluster = spc;
    g_vol.fat_count = fats;
    g_vol.fat_sectors = fatsz;
    g_vol.fat_lba = part + reserved;
    g_vol.data_lba = g_vol.fat_lba + fats * fatsz;
    g_vol.root_cluster = root_cluster;
    g_vol.total_clusters = (total - reserved - fats * fatsz) / spc;
    g_mounted = true;
    return SD_FAT_OK;
}

static sd_fat_result_t fat_get(uint32_t cluster, uint32_t *value) {
    if (cluster < 2u || cluster >= g_vol.total_clusters + 2u) {
        return SD_FAT_ERR_FORMAT;
    }
    uint32_t offset = cluster * 4u;
    sd_fat_result_t rc = card_read(g_vol.fat_lba + (offset >> 9), g_sector);
    if (rc != SD_FAT_OK) {
        return rc;
    }
    *value = rd32(&g_sector[offset & 0x1FFu]) & 0x0FFFFFFFu;
    return SD_FAT_OK;
}

static sd_fat_result_t fat_set(uint32_t cluster, uint32_t value) {
    if (cluster < 2u || cluster >= g_vol.total_clusters + 2u) {
        return SD_FAT_ERR_FORMAT;
    }
    uint32_t offset = cluster * 4u;
    for (uint8_t fat = 0; fat < g_vol.fat_count; ++fat) {
        uint32_t lba = g_vol.fat_lba + (uint32_t)fat * g_vol.fat_sectors + (offset >> 9);
        sd_fat_result_t rc = card_read(lba, g_sector);
        if (rc != SD_FAT_OK) {
            return rc;
        }
        wr32(&g_sector[offset & 0x1FFu], value & 0x0FFFFFFFu);
        rc = card_write(lba, g_sector);
        if (rc != SD_FAT_OK) {
            return rc;
        }
    }
    return SD_FAT_OK;
}

static sd_fat_result_t clear_cluster(uint32_t cluster) {
    for (uint32_t i = 0; i < 512u; ++i) {
        g_sector[i] = 0;
    }
    for (uint8_t s = 0; s < g_vol.sectors_per_cluster; ++s) {
        sd_fat_result_t rc = card_write(cluster_lba(cluster) + s, g_sector);
        if (rc != SD_FAT_OK) {
            return rc;
        }
    }
    return SD_FAT_OK;
}

static sd_fat_result_t alloc_cluster(uint32_t *cluster) {
    uint32_t value = 0;
    for (uint32_t c = 2u; c < g_vol.total_clusters + 2u; ++c) {
        sd_fat_result_t rc = fat_get(c, &value);
        if (rc != SD_FAT_OK) {
            return rc;
        }
        if (value == 0u) {
            rc = fat_set(c, 0x0FFFFFFFu);
            if (rc != SD_FAT_OK) {
                return rc;
            }
            rc = clear_cluster(c);
            if (rc != SD_FAT_OK) {
                return rc;
            }
            *cluster = c;
            return SD_FAT_OK;
        }
    }
    return SD_FAT_ERR_FULL;
}

static sd_fat_result_t next_or_alloc(uint32_t cluster, uint32_t *next) {
    uint32_t value = 0;
    sd_fat_result_t rc = fat_get(cluster, &value);
    if (rc != SD_FAT_OK) {
        return rc;
    }
    if (value >= 0x0FFFFFF8u) {
        uint32_t fresh = 0;
        rc = alloc_cluster(&fresh);
        if (rc != SD_FAT_OK) {
            return rc;
        }
        rc = fat_set(cluster, fresh);
        if (rc != SD_FAT_OK) {
            return rc;
        }
        *next = fresh;
    } else {
        *next = value;
    }
    return SD_FAT_OK;
}

static void make_dirent(uint8_t *e, const char name[11], uint32_t cluster) {
    for (uint8_t i = 0; i < 11u; ++i) {
        e[i] = (uint8_t)name[i];
    }
    e[11] = 0x20u;
    for (uint8_t i = 12u; i < 32u; ++i) {
        e[i] = 0;
    }
    wr16(&e[20], (uint16_t)(cluster >> 16));
    wr16(&e[26], (uint16_t)cluster);
    wr32(&e[28], 0u);
}

static sd_fat_result_t find_file(const char name[11], uint32_t *dir_lba, uint16_t *dir_off,
                                 uint32_t *first_cluster, uint32_t *size, bool *found) {
    uint32_t root = g_vol.root_cluster;
    *found = false;
    while (1) {
        for (uint8_t s = 0; s < g_vol.sectors_per_cluster; ++s) {
            uint32_t lba = cluster_lba(root) + s;
            sd_fat_result_t rc = card_read(lba, g_sector);
            if (rc != SD_FAT_OK) {
                return rc;
            }
            for (uint16_t off = 0; off < 512u; off += 32u) {
                uint8_t first = g_sector[off];
                if (first == 0x00u) {
                    return SD_FAT_OK;
                }
                if (first != 0xE5u &&
                    (g_sector[off + 11u] & 0x0Fu) != 0x0Fu &&
                    c_memeq(&g_sector[off], name, 11u)) {
                    *dir_lba = lba;
                    *dir_off = off;
                    *first_cluster = ((uint32_t)rd16(&g_sector[off + 20u]) << 16) |
                                     rd16(&g_sector[off + 26u]);
                    *size = rd32(&g_sector[off + 28u]);
                    *found = true;
                    return SD_FAT_OK;
                }
            }
        }
        uint32_t next = 0;
        sd_fat_result_t rc = fat_get(root, &next);
        if (rc != SD_FAT_OK) {
            return rc;
        }
        if (next >= 0x0FFFFFF8u) {
            return SD_FAT_OK;
        }
        root = next;
    }
}

static sd_fat_result_t find_or_create_file(const char name[11],
                                           uint32_t *dir_lba, uint16_t *dir_off,
                                           uint32_t *first_cluster, uint32_t *size) {
    uint32_t root = g_vol.root_cluster;
    uint32_t free_lba = 0;
    uint16_t free_off = 0;
    bool have_free = false;
    while (1) {
        for (uint8_t s = 0; s < g_vol.sectors_per_cluster; ++s) {
            uint32_t lba = cluster_lba(root) + s;
            sd_fat_result_t rc = card_read(lba, g_sector);
            if (rc != SD_FAT_OK) {
                return rc;
            }
            for (uint16_t off = 0; off < 512u; off += 32u) {
                uint8_t first = g_sector[off];
                if (first == 0x00u || first == 0xE5u) {
                    if (!have_free) {
                        free_lba = lba;
                        free_off = off;
                        have_free = true;
                    }
                    if (first == 0x00u) {
                        break;
                    }
                } else if ((g_sector[off + 11u] & 0x0Fu) != 0x0Fu &&
                           c_memeq(&g_sector[off], name, 11u)) {
                    *dir_lba = lba;
                    *dir_off = off;
                    *first_cluster = ((uint32_t)rd16(&g_sector[off + 20u]) << 16) |
                                     rd16(&g_sector[off + 26u]);
                    *size = rd32(&g_sector[off + 28u]);
                    return SD_FAT_OK;
                }
            }
        }
        uint32_t next = 0;
        sd_fat_result_t rc = fat_get(root, &next);
        if (rc != SD_FAT_OK) {
            return rc;
        }
        if (next >= 0x0FFFFFF8u) {
            if (have_free) {
                uint32_t fresh = 0;
                rc = alloc_cluster(&fresh);
                if (rc != SD_FAT_OK) {
                    return rc;
                }
                rc = card_read(free_lba, g_sector);
                if (rc != SD_FAT_OK) {
                    return rc;
                }
                make_dirent(&g_sector[free_off], name, fresh);
                rc = card_write(free_lba, g_sector);
                if (rc != SD_FAT_OK) {
                    return rc;
                }
                *dir_lba = free_lba;
                *dir_off = free_off;
                *first_cluster = fresh;
                *size = 0;
                return SD_FAT_OK;
            }
            rc = next_or_alloc(root, &next);
            if (rc != SD_FAT_OK) {
                return rc;
            }
        }
        root = next;
    }
}

static sd_fat_result_t update_dir_size(uint32_t dir_lba, uint16_t off, uint32_t first_cluster,
                                       uint32_t size) {
    sd_fat_result_t rc = card_read(dir_lba, g_sector);
    if (rc != SD_FAT_OK) {
        return rc;
    }
    wr16(&g_sector[off + 20u], (uint16_t)(first_cluster >> 16));
    wr16(&g_sector[off + 26u], (uint16_t)first_cluster);
    wr32(&g_sector[off + 28u], size);
    return card_write(dir_lba, g_sector);
}

static char append_byte_from_sources(const char *prefix, uint32_t prefix_len,
                                     const char *line, uint32_t index) {
    if (index < prefix_len) {
        return prefix[index];
    }
    return line[index - prefix_len];
}

static sd_fat_result_t append_named_internal(const char name[11],
                                             const char *header_if_empty,
                                             const char *line) {
    if (!sdio_sd_begin_session()) {
        g_mounted = false;
        g_last_card_error = SDIO_SD_ERR_TIMEOUT;
        return SD_FAT_ERR_CARD;
    }

    sd_fat_result_t rc = mount_fat32();
    if (rc != SD_FAT_OK) {
        close_card_session();
        return rc;
    }

    uint32_t dir_lba = 0;
    uint16_t dir_off = 0;
    uint32_t first_cluster = 0;
    uint32_t file_size = 0;
    rc = find_or_create_file(name, &dir_lba, &dir_off, &first_cluster, &file_size);
    if (rc != SD_FAT_OK) {
        close_card_session();
        return rc;
    }

    uint32_t header_len = (file_size == 0u && header_if_empty != 0) ?
                          c_strlen(header_if_empty) : 0u;
    uint32_t line_len = c_strlen(line);
    uint32_t len = header_len + line_len;
    if (len == 0u) {
        close_card_session();
        return SD_FAT_OK;
    }

    uint32_t cluster_bytes = (uint32_t)g_vol.sectors_per_cluster * 512u;
    uint32_t cluster = first_cluster;
    uint32_t remaining = file_size / cluster_bytes;
    while (remaining-- != 0u) {
        rc = next_or_alloc(cluster, &cluster);
        if (rc != SD_FAT_OK) {
            close_card_session();
            return rc;
        }
    }

    uint32_t pos = file_size % cluster_bytes;
    uint32_t written = 0;
    while (written < len) {
        uint8_t sector_in_cluster = (uint8_t)(pos >> 9);
        uint16_t sector_off = (uint16_t)(pos & 0x1FFu);
        uint32_t lba = cluster_lba(cluster) + sector_in_cluster;
        rc = card_read(lba, g_sector);
        if (rc != SD_FAT_OK) {
            close_card_session();
            return rc;
        }
        while (sector_off < 512u && written < len) {
            g_sector[sector_off++] =
                (uint8_t)append_byte_from_sources(header_if_empty, header_len, line, written++);
            ++pos;
            ++file_size;
        }
        rc = card_write(lba, g_sector);
        if (rc != SD_FAT_OK) {
            close_card_session();
            return rc;
        }
        if (pos >= cluster_bytes && written < len) {
            pos = 0;
            rc = next_or_alloc(cluster, &cluster);
            if (rc != SD_FAT_OK) {
                close_card_session();
                return rc;
            }
        }
    }

    rc = update_dir_size(dir_lba, dir_off, first_cluster, file_size);
    close_card_session();
    return rc;
}

sd_fat_result_t sd_fat_append_eevlog(const char *line) {
    static const char name[11] = {'E','E','V','L','O','G',' ',' ','C','S','V'};
    return append_named_internal(name, 0, line);
}

sd_fat_result_t sd_fat_append_eevlog_with_header(const char *header, const char *line) {
    static const char name[11] = {'E','E','V','L','O','G',' ',' ','C','S','V'};
    return append_named_internal(name, header, line);
}

sd_fat_result_t sd_fat_append_named_83_with_header(const char name83[11],
                                                   const char *header,
                                                   const char *line) {
    return append_named_internal(name83, header, line);
}

sd_fat_result_t sd_fat_file_exists_83(const char name83[11], bool *exists) {
    if (exists == 0) {
        return SD_FAT_ERR_IO;
    }
    *exists = false;
    if (!sdio_sd_begin_session()) {
        g_mounted = false;
        g_last_card_error = SDIO_SD_ERR_TIMEOUT;
        return SD_FAT_ERR_CARD;
    }
    sd_fat_result_t rc = mount_fat32();
    if (rc != SD_FAT_OK) {
        close_card_session();
        return rc;
    }
    uint32_t dir_lba = 0;
    uint16_t dir_off = 0;
    uint32_t first_cluster = 0;
    uint32_t size = 0;
    bool found = false;
    rc = find_file(name83, &dir_lba, &dir_off, &first_cluster, &size, &found);
    close_card_session();
    if (rc == SD_FAT_OK) {
        *exists = found;
    }
    return rc;
}

sd_fat_result_t sd_fat_read_named_83(const char name83[11],
                                     uint32_t offset,
                                     uint8_t *out,
                                     uint32_t max_len,
                                     uint32_t *out_len,
                                     uint32_t *out_size) {
    if (out == 0 || out_len == 0 || out_size == 0) {
        return SD_FAT_ERR_IO;
    }
    *out_len = 0;
    *out_size = 0;
    if (!sdio_sd_begin_session()) {
        g_mounted = false;
        g_last_card_error = SDIO_SD_ERR_TIMEOUT;
        return SD_FAT_ERR_CARD;
    }
    sd_fat_result_t rc = mount_fat32();
    if (rc != SD_FAT_OK) {
        close_card_session();
        return rc;
    }

    uint32_t dir_lba = 0;
    uint16_t dir_off = 0;
    uint32_t first_cluster = 0;
    uint32_t file_size = 0;
    bool found = false;
    rc = find_file(name83, &dir_lba, &dir_off, &first_cluster, &file_size, &found);
    (void)dir_lba;
    (void)dir_off;
    if (rc != SD_FAT_OK) {
        close_card_session();
        return rc;
    }
    if (!found) {
        close_card_session();
        return SD_FAT_ERR_IO;
    }
    *out_size = file_size;
    if (offset >= file_size || max_len == 0u) {
        close_card_session();
        return SD_FAT_OK;
    }
    if (max_len > file_size - offset) {
        max_len = file_size - offset;
    }

    uint32_t cluster_bytes = (uint32_t)g_vol.sectors_per_cluster * 512u;
    uint32_t cluster = first_cluster;
    uint32_t skip_clusters = offset / cluster_bytes;
    while (skip_clusters-- != 0u) {
        uint32_t next = 0;
        rc = fat_get(cluster, &next);
        if (rc != SD_FAT_OK) {
            close_card_session();
            return rc;
        }
        if (next >= 0x0FFFFFF8u) {
            close_card_session();
            return SD_FAT_ERR_FORMAT;
        }
        cluster = next;
    }

    uint32_t pos = offset % cluster_bytes;
    while (*out_len < max_len) {
        uint8_t sector_in_cluster = (uint8_t)(pos >> 9);
        uint16_t sector_off = (uint16_t)(pos & 0x1FFu);
        rc = card_read(cluster_lba(cluster) + sector_in_cluster, g_sector);
        if (rc != SD_FAT_OK) {
            close_card_session();
            return rc;
        }
        while (sector_off < 512u && *out_len < max_len) {
            out[*out_len] = g_sector[sector_off++];
            ++*out_len;
            ++pos;
        }
        if (pos >= cluster_bytes && *out_len < max_len) {
            uint32_t next = 0;
            rc = fat_get(cluster, &next);
            if (rc != SD_FAT_OK) {
                close_card_session();
                return rc;
            }
            if (next >= 0x0FFFFFF8u) {
                break;
            }
            cluster = next;
            pos = 0;
        }
    }
    close_card_session();
    return SD_FAT_OK;
}

uint8_t sd_fat_last_card_error(void) {
    return g_last_card_error;
}
