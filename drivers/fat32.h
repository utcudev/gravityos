/* =============================================================================
 * GravityOS — FAT32 (salt okunur)
 * =============================================================================
 * ATA sürücüsünün üstünde çalışır. Kök dizini listeler ve dosya okur.
 * Uzun dosya adı (LFN) desteği yok; adlar 8.3 biçiminde eşleştirilir.
 * ============================================================================= */

#ifndef DRIVERS_FAT32_H
#define DRIVERS_FAT32_H

#include <stdint.h>

/* Dosya sistemini tanı. Başarıda 1, aksi halde 0. */
int fat32_init(void);
int fat32_mounted(void);

/* Kök dizini ekrana listele. Dosya sayısını döner. */
int fat32_list_root(void);

/* Dosyayı buffer'a oku.
   Dönüş: okunan bayt sayısı, dosya yoksa -1, buffer küçükse -2. */
int fat32_read_file(const char *name, void *buffer, uint32_t buffer_size);

/* Dosya boyutunu döner, yoksa -1. */
int fat32_file_size(const char *name);

#endif /* DRIVERS_FAT32_H */
