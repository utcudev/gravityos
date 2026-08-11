/* =============================================================================
 * GravityOS — Ring 3 (user mode) geçişi
 * ============================================================================= */

#ifndef KERNEL_USERMODE_H
#define KERNEL_USERMODE_H

/* Test programını kullanıcı sayfalarına yükleyip ring 3'te çalıştırır.
   Program exit() syscall'ı yaptığında süreç sonlanır; geri dönmez. */
void usermode_run_test(void);

#endif /* KERNEL_USERMODE_H */
