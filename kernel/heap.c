/* =============================================================================
 * GravityOS — Kernel Heap Allocator (kmalloc / kfree)
 * =============================================================================
 * Serbest liste (free-list) tabanlı ayırıcı. Blokların başına bir başlık
 * konur; kfree komşu boş bloklarla birleştirme (coalescing) yapar, böylece
 * bellek gerçekten geri kazanılır.
 * ============================================================================= */

#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../lib/stdio.h"

/* Heap'in başlayacağı sanal adres (kernel alanının bittiği yerin ilerisi) */
#define HEAP_START_VIRT 0xFFFFFFFF81000000
#define HEAP_MAGIC      0x48454150U /* "HEAP" — bozulma tespiti için */
#define HEAP_ALIGN      16

typedef struct heap_block {
    uint32_t magic;
    uint32_t is_free;
    size_t   size;              /* Başlık hariç kullanılabilir bayt sayısı */
    struct heap_block *next;
    struct heap_block *prev;
} heap_block_t;

static uint64_t heap_mapped_limit = HEAP_START_VIRT; /* Haritalanmış son adres */
static heap_block_t *heap_head = NULL;
static heap_block_t *heap_tail = NULL;

static size_t align_up(size_t n)
{
    if (n % HEAP_ALIGN != 0) n += HEAP_ALIGN - (n % HEAP_ALIGN);
    return n;
}

/* Heap'i en az 'needed' bayt daha büyütecek kadar sayfa haritala.
   Yeni alanı serbest bir blok olarak listeye ekler. Başarısızlıkta 0 döner. */
static int heap_expand(size_t needed)
{
    size_t total = align_up(needed + sizeof(heap_block_t));
    uint64_t start = heap_mapped_limit;
    size_t mapped = 0;

    while (mapped < total) {
        void *phys = pmm_alloc_page();
        if (!phys) {
            kprintf("[HEAP] OUT OF MEMORY (expand failed)\n");
            return 0;
        }
        vmm_map_page((uint64_t)phys, heap_mapped_limit, PAGE_PRESENT | PAGE_WRITABLE);
        heap_mapped_limit += PMM_PAGE_SIZE;
        mapped += PMM_PAGE_SIZE;
    }

    heap_block_t *blk = (heap_block_t *)start;
    blk->magic   = HEAP_MAGIC;
    blk->is_free = 1;
    blk->size    = mapped - sizeof(heap_block_t);
    blk->next    = NULL;
    blk->prev    = heap_tail;

    if (heap_tail) {
        heap_tail->next = blk;
        /* Son blok da boşsa ikisini birleştir */
        if (heap_tail->is_free) {
            heap_tail->size += sizeof(heap_block_t) + blk->size;
            heap_tail->next = NULL;
            return 1;
        }
    } else {
        heap_head = blk;
    }

    heap_tail = blk;
    return 1;
}

void heap_init(void)
{
    heap_head = NULL;
    heap_tail = NULL;
    heap_mapped_limit = HEAP_START_VIRT;

    if (heap_expand(PMM_PAGE_SIZE)) {
        kprintf("[HEAP] Kernel heap initialized at 0x%lx\n", (uint64_t)HEAP_START_VIRT);
    } else {
        kprintf("[HEAP] ERROR: Failed to initialize heap!\n");
    }
}

/* Blok yeterince büyükse ikiye böl; kalan kısmı serbest blok yap */
static void split_block(heap_block_t *blk, size_t size)
{
    /* Bölme ancak kalan parça bir başlık + anlamlı alan tutabiliyorsa mantıklı */
    if (blk->size < size + sizeof(heap_block_t) + HEAP_ALIGN) return;

    heap_block_t *rest = (heap_block_t *)((uint8_t *)blk + sizeof(heap_block_t) + size);
    rest->magic   = HEAP_MAGIC;
    rest->is_free = 1;
    rest->size    = blk->size - size - sizeof(heap_block_t);
    rest->next    = blk->next;
    rest->prev    = blk;

    if (blk->next) blk->next->prev = rest;
    else           heap_tail = rest;

    blk->next = rest;
    blk->size = size;
}

void *kmalloc(size_t size)
{
    if (size == 0) return NULL;
    size = align_up(size);

    /* İlk uyan boş bloğu ara (first-fit) */
    for (int attempt = 0; attempt < 2; attempt++) {
        for (heap_block_t *blk = heap_head; blk; blk = blk->next) {
            if (blk->is_free && blk->size >= size) {
                split_block(blk, size);
                blk->is_free = 0;
                return (void *)((uint8_t *)blk + sizeof(heap_block_t));
            }
        }

        /* Uygun blok yok — heap'i büyüt ve bir kez daha dene */
        if (attempt == 0 && !heap_expand(size)) break;
    }

    kprintf("[HEAP] kmalloc(%lu) failed\n", (uint64_t)size);
    return NULL;
}

void kfree(void *ptr)
{
    if (!ptr) return;

    heap_block_t *blk = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));
    if (blk->magic != HEAP_MAGIC) {
        kprintf("[HEAP] kfree: invalid pointer 0x%lx (bad magic)\n", (uint64_t)ptr);
        return;
    }
    if (blk->is_free) {
        kprintf("[HEAP] kfree: double free at 0x%lx\n", (uint64_t)ptr);
        return;
    }

    blk->is_free = 1;

    /* Sonraki blokla birleştir */
    heap_block_t *next = blk->next;
    if (next && next->is_free &&
        (uint8_t *)next == (uint8_t *)blk + sizeof(heap_block_t) + blk->size) {
        blk->size += sizeof(heap_block_t) + next->size;
        blk->next = next->next;
        if (next->next) next->next->prev = blk;
        else            heap_tail = blk;
    }

    /* Önceki blokla birleştir */
    heap_block_t *prev = blk->prev;
    if (prev && prev->is_free &&
        (uint8_t *)blk == (uint8_t *)prev + sizeof(heap_block_t) + prev->size) {
        prev->size += sizeof(heap_block_t) + blk->size;
        prev->next = blk->next;
        if (blk->next) blk->next->prev = prev;
        else           heap_tail = prev;
    }
}
