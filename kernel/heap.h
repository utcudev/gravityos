/* =============================================================================
 * GravityOS — Kernel Heap Allocator (kmalloc / kfree)
 * =============================================================================
 * İşletim sistemi için dinamik bellek tahsisi sağlar.
 * PMM ve VMM üzerinden sayfa (page) talep ederek heap alanını büyütür.
 * ============================================================================= */

#ifndef KERNEL_HEAP_H
#define KERNEL_HEAP_H

#include <stdint.h>
#include <stddef.h>

void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif /* KERNEL_HEAP_H */
