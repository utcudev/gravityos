/* =============================================================================
 * GravityOS — Ring 3 (user mode) geçişi
 * =============================================================================
 * Kullanıcı sayfalarını hazırlar ve iretq ile ring 3'e atlar.
 * Ring 0'a dönüşün tek yolu `syscall` veya bir kesmedir.
 * ============================================================================= */

#include "usermode.h"
#include "pmm.h"
#include "vmm.h"
#include "process.h"
#include "../cpu/gdt.h"
#include "../lib/string.h"
#include "../lib/stdio.h"

/* Kernel içindeki test programının yerleştirileceği adres */
#define TEST_CODE_VIRT 0x0000000000400000ULL

/* usermode_prog.asm içindeki blob */
extern uint8_t user_prog_start[];
extern uint8_t user_prog_end[];

/* syscall_entry.asm bu değişkenden kernel stack tepesini okur */
extern uint64_t syscall_kernel_stack;

void usermode_reset_address_space(void)
{
    /* Önceki programdan kalan haritalamaları temizle. Fiziksel sayfalar süreç
       öldüğünde zaten geri verilmişti; burada kalan ölü girdileri siliyoruz. */
    for (uint64_t v = USER_REGION_START; v < USER_REGION_END; v += PMM_PAGE_SIZE) {
        if (vmm_is_mapped(v)) vmm_unmap_page(v);
    }
}

int usermode_map_range(uint64_t virt_start, uint64_t size)
{
    uint64_t start = virt_start & ~(PMM_PAGE_SIZE - 1);
    uint64_t end   = (virt_start + size + PMM_PAGE_SIZE - 1) & ~(PMM_PAGE_SIZE - 1);

    for (uint64_t v = start; v < end; v += PMM_PAGE_SIZE) {
        /* Aynı sayfaya düşen ikinci bir segment için yeniden ayırma yapma;
           yoksa önceki segmentin içeriği silinir. */
        if (vmm_is_mapped(v)) continue;

        void *phys = pmm_alloc_page();
        if (!phys) {
            kprintf("[USER] Out of memory mapping 0x%lx\n", v);
            return -1;
        }
        /* Süreç öldüğünde bu sayfalar otomatik geri verilsin */
        process_track_allocation(process_get_current_pid(), phys);
        vmm_map_page((uint64_t)phys, v, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        memset((void *)v, 0, PMM_PAGE_SIZE);
    }

    return 0;
}

void usermode_enter(uint64_t entry, uint64_t user_stack_top)
{
    /* Kesme veya syscall ring 3'ten geldiğinde kullanılacak kernel stack'i.
       TSS.rsp0 kesmeler için, syscall_kernel_stack `syscall` girişi için. */
    syscall_kernel_stack = tss_get_rsp0();

    kprintf("[USER] Entering ring 3 at 0x%lx\n", entry);

    /* iretq ile ring 3'e geç: SS, RSP, RFLAGS, CS, RIP sırayla stack'e konur.
       Seçicilerin RPL'i 3 olmalı, yoksa CPU #GP atar. */
    __asm__ volatile(
        "cli\n"
        "pushq %0\n"            /* SS  = user data | 3 */
        "pushq %1\n"            /* RSP = kullanıcı stack tepesi */
        "pushq $0x202\n"        /* RFLAGS: IF açık */
        "pushq %2\n"            /* CS  = user code | 3 */
        "pushq %3\n"            /* RIP = giriş noktası */
        "iretq\n"
        :
        : "r"((uint64_t)GDT_USER_DATA_RPL3),
          "r"(user_stack_top),
          "r"((uint64_t)GDT_USER_CODE_RPL3),
          "r"(entry)
        : "memory");

    __builtin_unreachable();
}

void usermode_run_test(void)
{
    uint64_t prog_size = (uint64_t)(user_prog_end - user_prog_start);

    kprintf("[USER] Loading built-in ring 3 program (%lu bytes)\n", prog_size);

    usermode_reset_address_space();

    if (usermode_map_range(TEST_CODE_VIRT, prog_size) != 0) return;
    if (usermode_map_range(USER_STACK_VIRT, USER_STACK_SIZE) != 0) return;

    memcpy((void *)TEST_CODE_VIRT, user_prog_start, prog_size);

    usermode_enter(TEST_CODE_VIRT, USER_STACK_VIRT + USER_STACK_SIZE - 16);
}
