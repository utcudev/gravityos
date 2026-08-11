/* =============================================================================
 * GravityOS — GDT + TSS kurulumu
 * ============================================================================= */

#include "gdt.h"
#include "../lib/string.h"
#include "../lib/stdio.h"

typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} gdt_entry_t;

/* TSS tanımlayıcısı 64-bit modda 16 bayttır — GDT'de iki slot kaplar */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} gdt_tss_entry_t;

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;      /* Ring 3'ten ring 0'a geçerken yüklenecek stack */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} tss_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} gdt_ptr_t;

/* 5 normal girdi + 2 slotluk TSS */
static gdt_entry_t gdt[7];
static gdt_ptr_t   gdtr;
static tss_t       tss;

/* Kesmeler ring 3'ten geldiğinde kullanılacak kernel stack'i */
#define KERNEL_STACK_SIZE 16384
static uint8_t kernel_stack[KERNEL_STACK_SIZE] __attribute__((aligned(16)));

static void gdt_set_entry(int idx, uint8_t access, uint8_t granularity)
{
    gdt[idx].limit_low   = 0xFFFF;
    gdt[idx].base_low    = 0;
    gdt[idx].base_mid    = 0;
    gdt[idx].access      = access;
    gdt[idx].granularity = granularity;
    gdt[idx].base_high   = 0;
}

void tss_set_rsp0(uint64_t rsp0) { tss.rsp0 = rsp0; }
uint64_t tss_get_rsp0(void)      { return tss.rsp0; }

void gdt_init(void)
{
    memset(gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    /* access baytı: P|DPL|S|Type, granularity: G|D/B|L|AVL */
    gdt_set_entry(1, 0x9A, 0xA0); /* kernel code, DPL 0, long mode */
    gdt_set_entry(2, 0x92, 0xC0); /* kernel data, DPL 0 */
    gdt_set_entry(3, 0xF2, 0xC0); /* user data,   DPL 3 */
    gdt_set_entry(4, 0xFA, 0xA0); /* user code,   DPL 3, long mode */

    /* TSS: kesme geldiğinde CPU rsp0'ı buradan okur */
    tss.rsp0 = (uint64_t)kernel_stack + KERNEL_STACK_SIZE;
    tss.iomap_base = sizeof(tss_t); /* I/O izin haritası yok */

    uint64_t tss_base = (uint64_t)&tss;
    gdt_tss_entry_t *tss_desc = (gdt_tss_entry_t *)&gdt[5];
    tss_desc->limit_low   = sizeof(tss_t) - 1;
    tss_desc->base_low    = tss_base & 0xFFFF;
    tss_desc->base_mid    = (tss_base >> 16) & 0xFF;
    tss_desc->access      = 0x89; /* Present, type 9 = 64-bit TSS (available) */
    tss_desc->granularity = 0x00;
    tss_desc->base_high   = (tss_base >> 24) & 0xFF;
    tss_desc->base_upper  = (uint32_t)(tss_base >> 32);
    tss_desc->reserved    = 0;

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)gdt;

    /* GDT'yi yükle, ardından uzak dönüş (lretq) ile CS'i tazele.
       CS doğrudan mov ile değiştirilemez. */
    __asm__ volatile(
        "lgdt %0\n"
        "pushq %1\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "mov %w2, %%ds\n"
        "mov %w2, %%es\n"
        "mov %w2, %%ss\n"
        "mov %w2, %%fs\n"
        "mov %w2, %%gs\n"
        :
        : "m"(gdtr), "i"((uint64_t)GDT_KERNEL_CODE), "r"((uint16_t)GDT_KERNEL_DATA)
        : "rax", "memory");

    /* TSS'i yükle */
    __asm__ volatile("ltr %w0" : : "r"((uint16_t)GDT_TSS));

    kprintf("[GDT] GDT and TSS loaded (kernel stack top 0x%lx)\n", tss.rsp0);
}
