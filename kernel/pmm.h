/* =============================================================================
 * GravityOS — Physical Memory Manager (PMM)
 * =============================================================================
 * Fiziksel RAM'i 4096 Byte (4KB) boyutundaki sayfalara (page) böler.
 * Hangi sayfanın dolu, hangisinin boş olduğunu bir Bitmap üzerinden takip eder.
 * ============================================================================= */

#ifndef KERNEL_PMM_H
#define KERNEL_PMM_H

#include <stdint.h>
#include <stddef.h>
#include "limine.h"

#define PMM_PAGE_SIZE 4096

void pmm_init(struct limine_memmap_response *mmap);
void* pmm_alloc_page(void);
void pmm_free_page(void* addr);

uint64_t pmm_get_free_memory(void);
uint64_t pmm_get_total_memory(void);

#endif /* KERNEL_PMM_H */
