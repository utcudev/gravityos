/* =============================================================================
 * GravityOS — Ring 3 (user mode) geçişi
 * ============================================================================= */

#ifndef KERNEL_USERMODE_H
#define KERNEL_USERMODE_H

#include <stdint.h>

/* Kernel imajının içindeki test programını ring 3'te çalıştırır. */
void usermode_run_test(void);

/* Ring 3'e geçer ve verilen adresten çalışmaya başlar. Geri dönmez. */
void usermode_enter(uint64_t entry, uint64_t user_stack_top);

/* Kullanıcı adres alanına sayfa haritala (kod/veri/stack için).
   Zaten haritalı sayfalara dokunmaz. */
int usermode_map_range(uint64_t virt_start, uint64_t size);

/* Önceki programdan kalan kullanıcı haritalamalarını temizle */
void usermode_reset_address_space(void);

/* Kullanıcı programlarına ayrılan sanal adres penceresi */
#define USER_REGION_START 0x0000000000400000ULL
#define USER_REGION_END   0x0000000000600000ULL

/* Kullanıcı stack'i için standart adres */
#define USER_STACK_VIRT 0x0000000000500000ULL
#define USER_STACK_SIZE 0x4000ULL

#endif /* KERNEL_USERMODE_H */
