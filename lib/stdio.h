/* =============================================================================
 * GravityOS — Standard I/O Header
 * ============================================================================= */

#ifndef LIB_STDIO_H
#define LIB_STDIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* Kernel printf — VGA ve serial'a yazar */
int kprintf(const char *fmt, ...);

/* Formatlı çıktı — buffer'a yazar */
int snprintf(char *buf, size_t size, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/* Temel I/O */
void kputchar(char c);
void kputs(const char *s);

#endif /* LIB_STDIO_H */
