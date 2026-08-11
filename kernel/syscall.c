/* =============================================================================
 * GravityOS — Syscall katmanı (Linux x86_64 numaralandırması)
 * =============================================================================
 * Ring 3'teki program `syscall` komutunu çalıştırdığında CPU doğrudan
 * syscall_entry (cpu/syscall_entry.asm) adresine atlar; o da buraya gelir.
 * ============================================================================= */

#include "syscall.h"
#include "process.h"
#include "../cpu/gdt.h"
#include "../lib/stdio.h"

/* MSR adresleri */
#define MSR_EFER   0xC0000080
#define MSR_STAR   0xC0000081
#define MSR_LSTAR  0xC0000082
#define MSR_SFMASK 0xC0000084

/* syscall_entry.asm buradan kernel stack tepesini okur */
uint64_t syscall_kernel_stack = 0;

extern void syscall_entry(void);

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t value)
{
    __asm__ volatile("wrmsr"
                     :
                     : "c"(msr), "a"((uint32_t)value), "d"((uint32_t)(value >> 32)));
}

void syscall_init(void)
{
    /* EFER.SCE (bit 0): syscall/sysret komutlarını etkinleştir */
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 1);

    /* STAR[47:32] = syscall sırasında yüklenecek kernel CS.
       STAR[63:48] = sysret tabanı; CPU CS'i taban+16, SS'i taban+8 yapar.
       Taban 0x10 seçilince CS=0x20 (user code), SS=0x18 (user data) olur. */
    uint64_t star = ((uint64_t)GDT_KERNEL_CODE << 32) |
                    ((uint64_t)GDT_KERNEL_DATA << 48);
    wrmsr(MSR_STAR, star);

    /* Giriş noktası */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* Syscall sırasında temizlenecek RFLAGS bitleri.
       IF'i (0x200) kapatıyoruz: kernel stack'i paylaşıldığı için syscall
       içindeyken kesme gelirse üzerine binerdi. DF (0x400) ve TF (0x100) de
       kapalı olmalı. */
    wrmsr(MSR_SFMASK, 0x700);

    kprintf("[SYSCALL] syscall/sysret enabled (entry 0x%lx)\n", (uint64_t)syscall_entry);
}

uint64_t syscall_handler(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                         uint64_t arg4, uint64_t arg5)
{
    (void)arg4;
    (void)arg5;

    switch (sys_num) {
    case SYS_WRITE: {
        int fd = (int)arg1;
        const char *buf = (const char *)arg2;
        uint64_t count = arg3;

        if (fd != 1 && fd != 2) return (uint64_t)-1;

        for (uint64_t i = 0; i < count; i++) {
            kputchar(buf[i]);
        }
        return count;
    }

    case SYS_EXIT: {
        int exit_code = (int)arg1;
        kprintf("[SYSCALL] Process %u exited with code %d\n",
                process_get_current_pid(), exit_code);
        process_exit(process_get_current_pid());
        return 0;
    }

    case SYS_BRK:
        /* Henüz gerçek bir heap büyütme yok; istenen adresi onaylıyoruz */
        return arg1;

    default:
        kprintf("[SYSCALL] Unknown syscall %lu\n", sys_num);
        return (uint64_t)-1; /* ENOSYS */
    }
}
