/* =============================================================================
 * GravityOS — Virtual Memory Manager (VMM - Paging for x86_64)
 * =============================================================================
 * 4 Seviyeli Paging (PML4, PDPT, PD, PT) kullanarak Sanal Adresleri (Virtual)
 * PMM'den alınan Fiziksel (Physical) adreslere eşler (map).
 * ============================================================================= */

#ifndef KERNEL_VMM_H
#define KERNEL_VMM_H

#include <stdint.h>
#include <stddef.h>

/* Page Table Entry Flags */
#define PAGE_PRESENT  0x01
#define PAGE_WRITABLE 0x02
#define PAGE_USER     0x04
#define PAGE_WRITETHROUGH 0x08
#define PAGE_NOCACHE      0x10  /* MMIO bölgeleri önbelleğe alınmamalı */
#define PAGE_HUGE     0x80

typedef uint64_t pt_entry_t;

void vmm_init(void);
void vmm_map_page(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags);
void vmm_unmap_page(uint64_t virt_addr);
int  vmm_is_mapped(uint64_t virt_addr);

uint64_t vmm_get_phys_addr(uint64_t virt_addr);

#endif /* KERNEL_VMM_H */
