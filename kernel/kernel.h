/* =============================================================================
 * GravityOS — Kernel Header
 * ============================================================================= */

#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

/* Bootloader'dan (Stage 2) gelen bilgi yapısı */
#define BOOT_MAGIC 0x47524156

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint64_t fb_addr;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;
    uint64_t memory_size;
} boot_info_t;

/* Kernel sürüm bilgisi */
#define GRAVITYOS_VERSION_MAJOR 0
#define GRAVITYOS_VERSION_MINOR 1
#define GRAVITYOS_VERSION_PATCH 0
#define GRAVITYOS_VERSION_STRING "0.1.0"
#define GRAVITYOS_NAME "GravityOS"

void kpanic(const char *fmt, ...);

#endif /* KERNEL_H */
