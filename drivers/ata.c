/* =============================================================================
 * GravityOS — ATA PIO Disk Sürücüsü (birincil kanal, master)
 * ============================================================================= */

#include "ata.h"
#include "../cpu/ports.h"
#include "../lib/stdio.h"
#include "../lib/string.h"

/* Birincil ATA kanalı port düzeni */
#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE       0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_ALT_STATUS  0x3F6

/* Status baytı bitleri */
#define ATA_SR_BSY  0x80  /* Meşgul */
#define ATA_SR_DRDY 0x40  /* Sürücü hazır */
#define ATA_SR_DRQ  0x08  /* Veri transferi isteniyor */
#define ATA_SR_ERR  0x01  /* Hata */

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_IDENTIFY 0xEC

static int      disk_present = 0;
static uint32_t disk_sectors = 0;
static char     disk_model[41] = "yok";

int      ata_present(void)      { return disk_present; }
uint32_t ata_sector_count(void) { return disk_sectors; }
const char *ata_model(void)     { return disk_model; }

/* Denetleyicinin durumu yansıtması için ~400ns bekle */
static void ata_io_wait(void)
{
    for (int i = 0; i < 4; i++) inb(ATA_ALT_STATUS);
}

/* BSY düşene kadar bekle. Zaman aşımında -1. */
static int ata_wait_not_busy(void)
{
    for (uint32_t i = 0; i < 4000000; i++) {
        if (!(inb(ATA_STATUS) & ATA_SR_BSY)) return 0;
    }
    return -1;
}

/* Veri hazır (DRQ) olana kadar bekle. Hata veya zaman aşımında -1. */
static int ata_wait_drq(void)
{
    for (uint32_t i = 0; i < 4000000; i++) {
        uint8_t status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return 0;
    }
    return -1;
}

int ata_init(void)
{
    disk_present = 0;

    /* Master sürücüyü seç */
    outb(ATA_DRIVE, 0xA0);
    ata_io_wait();

    /* IDENTIFY için tüm adres registerları sıfır olmalı */
    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA_LOW,  0);
    outb(ATA_LBA_MID,  0);
    outb(ATA_LBA_HIGH, 0);

    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    ata_io_wait();

    /* Status 0 ise bu kanalda sürücü yok */
    if (inb(ATA_STATUS) == 0) {
        kprintf("[ATA] No drive on primary channel.\n");
        return 0;
    }

    if (ata_wait_not_busy() != 0) {
        kprintf("[ATA] Drive stuck busy, giving up.\n");
        return 0;
    }

    /* LBA_MID/HIGH sıfır değilse ATA değil (ör. ATAPI/CD-ROM) */
    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HIGH) != 0) {
        kprintf("[ATA] Device is not a plain ATA disk (ATAPI?).\n");
        return 0;
    }

    if (ata_wait_drq() != 0) {
        kprintf("[ATA] IDENTIFY failed.\n");
        return 0;
    }

    uint16_t identify[256];
    for (int i = 0; i < 256; i++) {
        identify[i] = inw(ATA_DATA);
    }

    /* Model adı word 27-46, her word'de baytlar ters sırada */
    for (int i = 0; i < 20; i++) {
        disk_model[i * 2]     = (char)(identify[27 + i] >> 8);
        disk_model[i * 2 + 1] = (char)(identify[27 + i] & 0xFF);
    }
    disk_model[40] = '\0';
    for (int i = 39; i >= 0 && disk_model[i] == ' '; i--) disk_model[i] = '\0';

    /* LBA28 toplam sektör sayısı word 60-61 */
    disk_sectors = ((uint32_t)identify[61] << 16) | identify[60];

    disk_present = 1;
    kprintf("[ATA] Disk found: %s (%u sectors, %u MB)\n",
            disk_model, disk_sectors, disk_sectors / 2048);
    return 1;
}

int ata_read_sectors(uint32_t lba, uint8_t count, void *buffer)
{
    if (!disk_present) return -1;
    if (count == 0) return -1;

    uint16_t *out = (uint16_t *)buffer;

    if (ata_wait_not_busy() != 0) return -1;

    /* LBA28: üst 4 bit sürücü baytının içinde gider */
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_ERROR, 0);
    outb(ATA_SECCOUNT, count);
    outb(ATA_LBA_LOW,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID,  (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND,  ATA_CMD_READ_PIO);

    for (uint32_t s = 0; s < count; s++) {
        if (ata_wait_drq() != 0) {
            kprintf("[ATA] Read error at LBA %u\n", lba + s);
            return -1;
        }
        for (int i = 0; i < ATA_SECTOR_SIZE / 2; i++) {
            *out++ = inw(ATA_DATA);
        }
        ata_io_wait();
    }

    return 0;
}
