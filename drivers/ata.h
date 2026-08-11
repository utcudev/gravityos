/* =============================================================================
 * GravityOS — ATA PIO Disk Sürücüsü
 * =============================================================================
 * Birincil ATA kanalındaki master diski PIO (programlı G/Ç) modunda okur.
 * DMA yok: her sektör CPU tarafından 256 word olarak port üzerinden çekilir.
 * Yavaş ama basit ve her ortamda çalışır.
 * ============================================================================= */

#ifndef DRIVERS_ATA_H
#define DRIVERS_ATA_H

#include <stdint.h>

#define ATA_SECTOR_SIZE 512

/* Diski tanı. Bulunursa 1, bulunamazsa 0 döner. */
int ata_init(void);

/* Disk var mı ve kaç sektör? */
int      ata_present(void);
uint32_t ata_sector_count(void);
const char *ata_model(void);

/* lba'dan başlayarak count sektör oku. Başarıda 0, hatada -1. */
int ata_read_sectors(uint32_t lba, uint8_t count, void *buffer);

#endif /* DRIVERS_ATA_H */
