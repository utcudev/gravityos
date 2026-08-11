/* =============================================================================
 * GravityOS — Virtual Memory Manager (VMM - Paging)
 * ============================================================================= */

#include "vmm.h"
#include "pmm.h"
#include "../lib/stdio.h"
#include "../lib/string.h"

/* Şu anki PML4 (Page Map Level 4) tablosunun adresi */
static pt_entry_t* current_pml4 = NULL;

static inline void load_cr3(uint64_t phys_addr) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(phys_addr));
}

static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

extern uint64_t hhdm_offset;

void vmm_init(void)
{
    /* Limine CR3 loaded (mask out flags) */
    uint64_t phys_pml4 = read_cr3() & ~0xFFFULL;
    
    /* Access page tables via Higher Half Direct Map */
    current_pml4 = (pt_entry_t*)(phys_pml4 + hhdm_offset);

    kprintf("[VMM] Virtual Memory Manager Initialized.\n");
    kprintf("[VMM] Active PML4 Table at Physical: 0x%lx\n", phys_pml4);
}

void vmm_map_page(uint64_t phys_addr, uint64_t virt_addr, uint64_t flags)
{
    /* x86_64 Sanal Adres Formatı:
       PML4_IDX: Bits 39-47
       PDPT_IDX: Bits 30-38
       PD_IDX:   Bits 21-29
       PT_IDX:   Bits 12-20
    */
    uint64_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt_addr >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt_addr >> 12) & 0x1FF;

    /* Ara tablolar her zaman yazılabilir olmalı; erişim izni en alttaki
       PT girdisinden belirlenir. USER bayrağı ise zincirin tamamında gerekir. */
    uint64_t table_flags = PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);

    if (!(current_pml4[pml4_idx] & PAGE_PRESENT)) {
        void* new_pdpt = pmm_alloc_page();
        if (!new_pdpt) { kprintf("[VMM] Out of memory mapping 0x%lx\n", virt_addr); return; }
        memset((void*)((uint64_t)new_pdpt + hhdm_offset), 0, PMM_PAGE_SIZE);
        current_pml4[pml4_idx] = (uint64_t)new_pdpt | table_flags;
    }

    pt_entry_t* pdpt = (pt_entry_t*)((current_pml4[pml4_idx] & ~0xFFF) + hhdm_offset);
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        void* new_pd = pmm_alloc_page();
        if (!new_pd) { kprintf("[VMM] Out of memory mapping 0x%lx\n", virt_addr); return; }
        memset((void*)((uint64_t)new_pd + hhdm_offset), 0, PMM_PAGE_SIZE);
        pdpt[pdpt_idx] = (uint64_t)new_pd | table_flags;
    }

    pt_entry_t* pd = (pt_entry_t*)((pdpt[pdpt_idx] & ~0xFFF) + hhdm_offset);
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        void* new_pt = pmm_alloc_page();
        if (!new_pt) { kprintf("[VMM] Out of memory mapping 0x%lx\n", virt_addr); return; }
        memset((void*)((uint64_t)new_pt + hhdm_offset), 0, PMM_PAGE_SIZE);
        pd[pd_idx] = (uint64_t)new_pt | table_flags;
    }

    pt_entry_t* pt = (pt_entry_t*)((pd[pd_idx] & ~0xFFF) + hhdm_offset);
    
    /* İlgili PT_IDX'e fiziksel adresi eşle */
    pt[pt_idx] = phys_addr | flags | PAGE_PRESENT;

    /* TLB Cache'i temizle */
    __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

int vmm_is_mapped(uint64_t virt_addr)
{
    uint64_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt_addr >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt_addr >> 12) & 0x1FF;

    if (!(current_pml4[pml4_idx] & PAGE_PRESENT)) return 0;
    pt_entry_t* pdpt = (pt_entry_t*)((current_pml4[pml4_idx] & ~0xFFF) + hhdm_offset);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
    pt_entry_t* pd = (pt_entry_t*)((pdpt[pdpt_idx] & ~0xFFF) + hhdm_offset);

    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    /* 2 MB büyük sayfa ise zaten haritalı sayılır */
    if (pd[pd_idx] & PAGE_HUGE) return 1;

    pt_entry_t* pt = (pt_entry_t*)((pd[pd_idx] & ~0xFFF) + hhdm_offset);
    return (pt[pt_idx] & PAGE_PRESENT) ? 1 : 0;
}

void vmm_unmap_page(uint64_t virt_addr)
{
    /* Basit unmap */
    uint64_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt_addr >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt_addr >> 12) & 0x1FF;

    if (!(current_pml4[pml4_idx] & PAGE_PRESENT)) return;
    pt_entry_t* pdpt = (pt_entry_t*)((current_pml4[pml4_idx] & ~0xFFF) + hhdm_offset);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return;
    pt_entry_t* pd = (pt_entry_t*)((pdpt[pdpt_idx] & ~0xFFF) + hhdm_offset);

    if (!(pd[pd_idx] & PAGE_PRESENT)) return;
    pt_entry_t* pt = (pt_entry_t*)((pd[pd_idx] & ~0xFFF) + hhdm_offset);

    pt[pt_idx] = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
}
