/* =============================================================================
 * GravityOS — Ring 3 (user mode) geçişi
 * =============================================================================
 * Test programını kullanıcı erişimine açık sayfalara kopyalar ve iretq ile
 * ring 3'e atlar. Ring 0'a dönüşün tek yolu `syscall` veya bir kesmedir.
 * ============================================================================= */

#include "usermode.h"
#include "pmm.h"
#include "vmm.h"
#include "process.h"
#include "../cpu/gdt.h"
#include "../lib/string.h"
#include "../lib/stdio.h"

/* Kullanıcı adres alanındaki sabit yerleşim */
#define USER_CODE_VIRT  0x0000000000400000ULL
#define USER_STACK_VIRT 0x0000000000500000ULL

/* usermode_prog.asm içindeki blob */
extern uint8_t user_prog_start[];
extern uint8_t user_prog_end[];

/* syscall_entry.asm bu değişkenden kernel stack tepesini okur */
extern uint64_t syscall_kernel_stack;

void usermode_run_test(void)
{
    uint64_t prog_size = (uint64_t)(user_prog_end - user_prog_start);

    kprintf("[USER] Loading ring 3 program (%lu bytes) at 0x%lx\n",
            prog_size, (uint64_t)USER_CODE_VIRT);

    /* Kod sayfası — kullanıcıdan erişilebilir olmalı (PAGE_USER) */
    void *code_phys = pmm_alloc_page();
    if (!code_phys) { kprintf("[USER] No memory for code page\n"); return; }
    process_track_allocation(process_get_current_pid(), code_phys);
    vmm_map_page((uint64_t)code_phys, USER_CODE_VIRT,
                 PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    /* Kullanıcı stack'i */
    void *stack_phys = pmm_alloc_page();
    if (!stack_phys) { kprintf("[USER] No memory for stack page\n"); return; }
    process_track_allocation(process_get_current_pid(), stack_phys);
    vmm_map_page((uint64_t)stack_phys, USER_STACK_VIRT,
                 PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    memcpy((void *)USER_CODE_VIRT, user_prog_start, prog_size);
    memset((void *)USER_STACK_VIRT, 0, PMM_PAGE_SIZE);

    /* Kesme veya syscall ring 3'ten geldiğinde kullanılacak kernel stack'i.
       TSS.rsp0 kesmeler için, syscall_kernel_stack `syscall` girişi için. */
    uint64_t kstack_top = tss_get_rsp0();
    syscall_kernel_stack = kstack_top;

    uint64_t user_stack_top = (USER_STACK_VIRT + PMM_PAGE_SIZE - 16);

    kprintf("[USER] Entering ring 3...\n");

    /* iretq ile ring 3'e geç: stack'e SS, RSP, RFLAGS, CS, RIP koyuyoruz.
       Seçicilerin RPL'i 3 olmalı, yoksa CPU #GP atar. */
    __asm__ volatile(
        "cli\n"
        "pushq %0\n"            /* SS  = user data | 3 */
        "pushq %1\n"            /* RSP = kullanıcı stack tepesi */
        "pushq $0x202\n"        /* RFLAGS: IF açık (ring 3'te kesmeler gelsin) */
        "pushq %2\n"            /* CS  = user code | 3 */
        "pushq %3\n"            /* RIP = programın giriş noktası */
        "iretq\n"
        :
        : "r"((uint64_t)GDT_USER_DATA_RPL3),
          "r"(user_stack_top),
          "r"((uint64_t)GDT_USER_CODE_RPL3),
          "r"((uint64_t)USER_CODE_VIRT)
        : "memory");

    __builtin_unreachable();
}
