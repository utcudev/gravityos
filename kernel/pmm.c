/* =============================================================================
 * GravityOS — Physical Memory Manager (PMM)
 * ============================================================================= */

#include "pmm.h"
#include "../lib/stdio.h"
#include "../lib/string.h"

/* Bitmap: Her bit 1 sayfa (4KB) temsil eder. 1 = Dolu, 0 = Boş */
/* 512MB RAM'e kadar desteklemek için: 512MB / 4KB = 131072 Sayfa -> 16384 Byte Bitmap */
#define BITMAP_SIZE 16384
static uint8_t pmm_bitmap[BITMAP_SIZE];

static uint64_t total_memory = 0;
static uint64_t free_memory = 0;
static uint64_t total_pages = 0;

static inline void bitmap_set(uint64_t bit) {
    pmm_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(uint64_t bit) {
    pmm_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline int bitmap_test(uint64_t bit) {
    return (pmm_bitmap[bit / 8] & (1 << (bit % 8)));
}

void pmm_init(struct limine_memmap_response *mmap)
{
    /* Bitmap boyutu için en yüksek KULLANILABİLİR adresi baz al.
       Tüm girdilerin tepesi alınırsa framebuffer gibi MMIO bölgeleri
       yüzünden RAM miktarı gigabaytlarca fazla görünür. */
    uint64_t highest_addr = 0;
    uint64_t usable_memory = 0;
    for (uint64_t i = 0; i < mmap->entry_count; i++) {
        if (mmap->entries[i]->type != LIMINE_MEMMAP_USABLE) continue;
        uint64_t top = mmap->entries[i]->base + mmap->entries[i]->length;
        if (top > highest_addr) highest_addr = top;
        usable_memory += mmap->entries[i]->length;
    }

    total_memory = usable_memory;
    total_pages = highest_addr / PMM_PAGE_SIZE;
    if (total_pages > BITMAP_SIZE * 8) {
        total_pages = BITMAP_SIZE * 8; /* Bitmap kapasitesi ile sınırla */
        kprintf("[PMM] WARNING: RAM exceeds bitmap capacity (%lu MB); rest unused.\n",
                (uint64_t)(BITMAP_SIZE * 8 * PMM_PAGE_SIZE) / (1024 * 1024));
    }

    /* Başlangıçta tüm hafızayı DOLU (1) olarak işaretle */
    memset(pmm_bitmap, 0xFF, BITMAP_SIZE);
    free_memory = 0;

    /* Sadece USABLE olan bölgeleri BOŞ (0) yap */
    for (uint64_t i = 0; i < mmap->entry_count; i++) {
        if (mmap->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            uint64_t base = mmap->entries[i]->base;
            uint64_t length = mmap->entries[i]->length;
            for (uint64_t j = 0; j < length; j += PMM_PAGE_SIZE) {
                uint64_t page_idx = (base + j) / PMM_PAGE_SIZE;
                if (page_idx < total_pages) {
                    bitmap_clear(page_idx);
                    free_memory += PMM_PAGE_SIZE;
                }
            }
        }
    }

    /* İlk 1 MB'ı kullanım dışı bırak: fiziksel adres 0 ayrılırsa çağıranlar
       bunu NULL (bellek bitti) sanar, ayrıca 0-1MB'da BIOS/legacy yapılar var. */
    for (uint64_t page = 0; page < (0x100000 / PMM_PAGE_SIZE) && page < total_pages; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            free_memory -= PMM_PAGE_SIZE;
        }
    }

    kprintf("[PMM] Physical Memory Manager Initialized.\n");
    kprintf("[PMM] Total Physical Memory space: %lu MB, Free: %lu MB\n", total_memory / (1024 * 1024), free_memory / (1024 * 1024));
}

void* pmm_alloc_page(void)
{
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_memory -= PMM_PAGE_SIZE;
            return (void*)(i * PMM_PAGE_SIZE); /* Fiziksel Adres */
        }
    }
    kprintf("[PMM] OUT OF MEMORY!\n");
    return NULL;
}

void pmm_free_page(void* addr)
{
    uint64_t page = (uint64_t)addr / PMM_PAGE_SIZE;
    if (bitmap_test(page)) {
        bitmap_clear(page);
        free_memory += PMM_PAGE_SIZE;
    }
}

uint64_t pmm_get_free_memory(void) { return free_memory; }
uint64_t pmm_get_total_memory(void) { return total_memory; }
