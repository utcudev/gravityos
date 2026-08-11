/* =============================================================================
 * GravityOS — GDT + TSS
 * =============================================================================
 * Limine kendi GDT'siyle bırakıyor; içinde ne user segmenti ne de TSS var.
 * Ring 3'e geçmek ve kesme anında ring 0 stack'ine dönebilmek için kendi
 * GDT'mizi kurmak zorundayız.
 *
 * Seçici düzeni syscall/sysret'in beklediği sıraya göre:
 *   0x00 null
 *   0x08 kernel code (64-bit)
 *   0x10 kernel data
 *   0x18 user data     (sysret SS = STAR_base + 8)
 *   0x20 user code     (sysret CS = STAR_base + 16)
 *   0x28 TSS (16 bayt, iki slot kaplar)
 * ============================================================================= */

#ifndef CPU_GDT_H
#define CPU_GDT_H

#include <stdint.h>

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   0x18
#define GDT_USER_CODE   0x20
#define GDT_TSS         0x28

/* Ring 3'e geçerken RPL bitleri eklenir */
#define GDT_USER_CODE_RPL3 (GDT_USER_CODE | 3)
#define GDT_USER_DATA_RPL3 (GDT_USER_DATA | 3)

void gdt_init(void);

/* Kesme/syscall sırasında kullanılacak ring 0 stack'ini ayarla */
void tss_set_rsp0(uint64_t rsp0);
uint64_t tss_get_rsp0(void);

#endif /* CPU_GDT_H */
