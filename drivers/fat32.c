/* =============================================================================
 * GravityOS — FAT32 (salt okunur)
 * ============================================================================= */

#include "fat32.h"
#include "ata.h"
#include "../lib/stdio.h"
#include "../lib/string.h"

#define DIR_ENTRY_SIZE   32
#define ATTR_VOLUME_ID   0x08
#define ATTR_DIRECTORY   0x10
#define ATTR_LFN         0x0F
#define FAT_EOC          0x0FFFFFF8

static struct {
    int      mounted;
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint32_t fat_size;          /* sektör cinsinden, FAT başına */
    uint32_t root_cluster;
    uint32_t first_data_sector;
} fs;

/* Tek sektörlük paylaşılan tampon — kesme bağlamından çağrılmamalı */
static uint8_t sector_buf[ATA_SECTOR_SIZE];

int fat32_mounted(void) { return fs.mounted; }

static uint32_t cluster_to_lba(uint32_t cluster)
{
    return fs.first_data_sector + (cluster - 2) * fs.sectors_per_cluster;
}

/* FAT zincirinde bir sonraki küme */
static uint32_t fat_next_cluster(uint32_t cluster)
{
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs.reserved_sectors + (fat_offset / fs.bytes_per_sector);
    uint32_t entry_off  = fat_offset % fs.bytes_per_sector;

    if (ata_read_sectors(fat_sector, 1, sector_buf) != 0) return FAT_EOC;

    uint32_t value;
    memcpy(&value, &sector_buf[entry_off], 4);
    return value & 0x0FFFFFFF;
}

int fat32_init(void)
{
    fs.mounted = 0;

    if (!ata_present()) {
        kprintf("[FAT32] No disk, skipping mount.\n");
        return 0;
    }

    if (ata_read_sectors(0, 1, sector_buf) != 0) {
        kprintf("[FAT32] Could not read boot sector.\n");
        return 0;
    }

    if (sector_buf[510] != 0x55 || sector_buf[511] != 0xAA) {
        kprintf("[FAT32] No boot signature, not a FAT volume.\n");
        return 0;
    }

    uint16_t bps;
    memcpy(&bps, &sector_buf[11], 2);
    if (bps != ATA_SECTOR_SIZE) {
        kprintf("[FAT32] Unsupported sector size %u\n", bps);
        return 0;
    }

    fs.bytes_per_sector    = bps;
    fs.sectors_per_cluster = sector_buf[13];
    memcpy(&fs.reserved_sectors, &sector_buf[14], 2);
    fs.num_fats            = sector_buf[16];
    memcpy(&fs.fat_size,     &sector_buf[36], 4);
    memcpy(&fs.root_cluster, &sector_buf[44], 4);

    if (fs.sectors_per_cluster == 0 || fs.fat_size == 0) {
        kprintf("[FAT32] Invalid BPB (not FAT32?).\n");
        return 0;
    }

    fs.first_data_sector = fs.reserved_sectors + fs.num_fats * fs.fat_size;
    fs.mounted = 1;

    kprintf("[FAT32] Mounted: %u bytes/sector, %u sectors/cluster, data at LBA %u\n",
            fs.bytes_per_sector, fs.sectors_per_cluster, fs.first_data_sector);
    return 1;
}

/* "hello.txt" -> "HELLO   TXT" (11 bayt, boşlukla doldurulmuş) */
static void to_short_name(const char *name, char out[11])
{
    memset(out, ' ', 11);

    int i = 0;
    while (name[i] && name[i] != '.' && i < 8) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[i] = c;
        i++;
    }

    const char *dot = strchr(name, '.');
    if (!dot) return;

    for (int j = 0; j < 3 && dot[j + 1]; j++) {
        char c = dot[j + 1];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[8 + j] = c;
    }
}

/* Kök dizinde ada göre girdi ara. Bulursa 1 döner, küme ve boyutu yazar. */
static int find_entry(const char *name, uint32_t *out_cluster, uint32_t *out_size)
{
    char target[11];
    to_short_name(name, target);

    uint32_t cluster = fs.root_cluster;

    while (cluster >= 2 && cluster < FAT_EOC) {
        for (uint32_t s = 0; s < fs.sectors_per_cluster; s++) {
            if (ata_read_sectors(cluster_to_lba(cluster) + s, 1, sector_buf) != 0) return 0;

            for (int off = 0; off < ATA_SECTOR_SIZE; off += DIR_ENTRY_SIZE) {
                uint8_t *e = &sector_buf[off];

                if (e[0] == 0x00) return 0;      /* dizin sonu */
                if (e[0] == 0xE5) continue;      /* silinmiş */
                if (e[11] == ATTR_LFN) continue; /* uzun ad parçası */
                if (e[11] & ATTR_VOLUME_ID) continue;

                if (memcmp(e, target, 11) == 0) {
                    uint16_t hi, lo;
                    memcpy(&hi, &e[20], 2);
                    memcpy(&lo, &e[26], 2);
                    *out_cluster = ((uint32_t)hi << 16) | lo;
                    memcpy(out_size, &e[28], 4);
                    return 1;
                }
            }
        }
        cluster = fat_next_cluster(cluster);
    }

    return 0;
}

int fat32_list_root(void)
{
    if (!fs.mounted) {
        kprintf("fat32: not mounted\n");
        return -1;
    }

    uint32_t cluster = fs.root_cluster;
    int count = 0;

    while (cluster >= 2 && cluster < FAT_EOC) {
        for (uint32_t s = 0; s < fs.sectors_per_cluster; s++) {
            if (ata_read_sectors(cluster_to_lba(cluster) + s, 1, sector_buf) != 0) return -1;

            for (int off = 0; off < ATA_SECTOR_SIZE; off += DIR_ENTRY_SIZE) {
                uint8_t *e = &sector_buf[off];

                if (e[0] == 0x00) return count;
                if (e[0] == 0xE5) continue;
                if (e[11] == ATTR_LFN) continue;
                if (e[11] & ATTR_VOLUME_ID) continue;

                /* 8.3 adını okunur biçime çevir */
                char name[16]; /* 12 karakter ad + 14'e kadar dolgu + '\0' */
                int n = 0;
                for (int i = 0; i < 8 && e[i] != ' '; i++) name[n++] = (char)e[i];
                if (e[8] != ' ') {
                    name[n++] = '.';
                    for (int i = 8; i < 11 && e[i] != ' '; i++) name[n++] = (char)e[i];
                }
                /* kprintf sola yaslama (%-14s) desteklemiyor, elle doldur */
                while (n < 14) name[n++] = ' ';
                name[n] = '\0';

                uint32_t size;
                memcpy(&size, &e[28], 4);

                if (e[11] & ATTR_DIRECTORY) {
                    kprintf("  %s <DIR>\n", name);
                } else {
                    kprintf("  %s %lu bytes\n", name, (uint64_t)size);
                }
                count++;
            }
        }
        cluster = fat_next_cluster(cluster);
    }

    return count;
}

int fat32_file_size(const char *name)
{
    if (!fs.mounted) return -1;

    uint32_t cluster, size;
    if (!find_entry(name, &cluster, &size)) return -1;
    return (int)size;
}

int fat32_read_file(const char *name, void *buffer, uint32_t buffer_size)
{
    if (!fs.mounted) return -1;

    uint32_t cluster, size;
    if (!find_entry(name, &cluster, &size)) return -1;
    if (size > buffer_size) return -2;

    uint8_t *out = (uint8_t *)buffer;
    uint32_t remaining = size;

    while (cluster >= 2 && cluster < FAT_EOC && remaining > 0) {
        for (uint32_t s = 0; s < fs.sectors_per_cluster && remaining > 0; s++) {
            if (ata_read_sectors(cluster_to_lba(cluster) + s, 1, sector_buf) != 0) return -1;

            uint32_t chunk = remaining < ATA_SECTOR_SIZE ? remaining : ATA_SECTOR_SIZE;
            memcpy(out, sector_buf, chunk);
            out += chunk;
            remaining -= chunk;
        }
        cluster = fat_next_cluster(cluster);
    }

    return (int)(size - remaining);
}
