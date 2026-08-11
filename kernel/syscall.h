/* =============================================================================
 * GravityOS — Linux Syscall Emulator
 * =============================================================================
 * Debian/Linux uygulamaları çalıştırıldığında işletim sistemine istek
 * göndermek için 'syscall' komutunu kullanır. Bu dosya Linux'un sistem
 * çağrılarını (sys_write, sys_exit vb.) taklit ederek uygulamayı çalıştırır.
 * ============================================================================= */

#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <stdint.h>

/* Linux x86_64 Syscall Numaraları */
#define SYS_READ  0
#define SYS_WRITE 1
#define SYS_OPEN  2
#define SYS_CLOSE 3
#define SYS_MMAP  9
#define SYS_BRK   12
#define SYS_EXIT  60

void syscall_init(void);
uint64_t syscall_handler(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

#endif /* KERNEL_SYSCALL_H */
