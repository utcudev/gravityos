/* =============================================================================
 * GravityOS — Linux Syscall Emulator Implementation
 * ============================================================================= */

#include "syscall.h"
#include "../lib/stdio.h"
#include "../drivers/font.h"
#include "process.h"

/* Terminal çizim konumları */
static uint32_t term_cursor_x = 160; /* Window X + 10 */
static uint32_t term_cursor_y = 170; /* Window Y + 70 */

/* ==========================================================================
 * syscall_handler — Debian/Linux uygulaması syscall yaptığında buraya düşer
 * ========================================================================== */
uint64_t syscall_handler(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    (void)arg4;
    (void)arg5;

    switch (sys_num) {
        case SYS_WRITE: {
            int fd = (int)arg1;
            char *buf = (char *)arg2;
            uint64_t count = arg3;
            
            if (fd == 1 || fd == 2) { /* stdout veya stderr */
                /* Linux uygulaması ekrana bir şey yazdırmak istediğinde
                 * biz onu alıp kendi GUI penceremize çiziyoruz! */
                for (uint64_t i = 0; i < count; i++) {
                    if (buf[i] == '\n') {
                        term_cursor_x = 160;
                        term_cursor_y += 10;
                    } else {
                        font_draw_char(term_cursor_x, term_cursor_y, buf[i], FB_COLOR(200, 200, 200), 0);
                        term_cursor_x += 8;
                    }
                }
                return count;
            }
            return -1;
        }

        case SYS_EXIT: {
            int exit_code = (int)arg1;
            kprintf("[ELF] Application exited with code %d\n", exit_code);
            process_exit(process_get_current_pid());
            return 0;
        }

        case SYS_BRK: {
            /* Çok basit bellek ayırma taklidi (memory allocation) */
            kprintf("[ELF] Syscall BRK requested\n");
            return arg1; /* Şimdilik başarılı dönüyoruz */
        }

        default:
            kprintf("[ELF] Unknown syscall %lu called!\n", sys_num);
            return -1; /* ENOSYS */
    }
}
