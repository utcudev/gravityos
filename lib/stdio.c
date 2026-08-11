/* =============================================================================
 * GravityOS — Standard I/O Implementation
 * =============================================================================
 * kprintf: kernel-level printf fonksiyonu
 * VGA ekranına ve serial port'a çıktı yazar.
 * ============================================================================= */

#include "stdio.h"
#include "string.h"
#include "../drivers/fbcon.h"
#include "../drivers/serial.h"

/* ==========================================================================
 * vsnprintf — Formatlı çıktıyı buffer'a yaz
 * Desteklenen formatlar: %d, %u, %x, %X, %p, %s, %c, %%, %ld, %lu, %lx
 * ========================================================================== */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    size_t pos = 0;
    char tmp[65]; /* 64-bit sayı için yeterli */

    #define PUT(c) do { if (pos < size - 1) buf[pos] = (c); pos++; } while(0)

    while (*fmt) {
        if (*fmt != '%') {
            PUT(*fmt);
            fmt++;
            continue;
        }

        fmt++; /* '%' atla */

        /* Flag'lar */
        int pad_zero = 0;
        int pad_width = 0;
        int is_long = 0;

        if (*fmt == '0') {
            pad_zero = 1;
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9') {
            pad_width = pad_width * 10 + (*fmt - '0');
            fmt++;
        }

        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
        }

        switch (*fmt) {
        case 'd':
        case 'i': {
            int64_t val;
            if (is_long)
                val = va_arg(args, int64_t);
            else
                val = va_arg(args, int);

            if (val < 0) {
                PUT('-');
                val = -val;
            }
            utoa((uint64_t)val, tmp, 10);

            /* Padding */
            int len = (int)strlen(tmp);
            for (int i = 0; i < pad_width - len; i++)
                PUT(pad_zero ? '0' : ' ');

            for (int i = 0; tmp[i]; i++)
                PUT(tmp[i]);
            break;
        }
        case 'u': {
            uint64_t val;
            if (is_long)
                val = va_arg(args, uint64_t);
            else
                val = va_arg(args, uint32_t);
            utoa(val, tmp, 10);

            int len = (int)strlen(tmp);
            for (int i = 0; i < pad_width - len; i++)
                PUT(pad_zero ? '0' : ' ');

            for (int i = 0; tmp[i]; i++)
                PUT(tmp[i]);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t val;
            if (is_long)
                val = va_arg(args, uint64_t);
            else
                val = va_arg(args, uint32_t);
            utoa(val, tmp, 16);

            int len = (int)strlen(tmp);
            for (int i = 0; i < pad_width - len; i++)
                PUT(pad_zero ? '0' : ' ');

            for (int i = 0; tmp[i]; i++)
                PUT(tmp[i]);
            break;
        }
        case 'p': {
            uint64_t val = (uint64_t)va_arg(args, void *);
            PUT('0');
            PUT('x');
            utoa(val, tmp, 16);

            int len = (int)strlen(tmp);
            for (int i = 0; i < 16 - len; i++)
                PUT('0');

            for (int i = 0; tmp[i]; i++)
                PUT(tmp[i]);
            break;
        }
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";

            int len = (int)strlen(s);
            for (int i = 0; i < pad_width - len; i++)
                PUT(' ');

            while (*s)
                PUT(*s++);
            break;
        }
        case 'c': {
            char c = (char)va_arg(args, int);
            PUT(c);
            break;
        }
        case '%':
            PUT('%');
            break;
        default:
            PUT('%');
            PUT(*fmt);
            break;
        }

        fmt++;
    }

    if (size > 0) {
        buf[pos < size ? pos : size - 1] = '\0';
    }

    #undef PUT
    return (int)pos;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}

/* ==========================================================================
 * kputchar — Tek karakter yaz (Framebuffer konsolu + Serial)
 * =============================================================================
 * Konsol henüz kurulmadıysa fbcon_putchar sessizce döner; erken boot
 * mesajları yine de seri porttan okunabilir.
 * ========================================================================== */
void kputchar(char c)
{
    fbcon_putchar(c);
    serial_putchar(c);
}

/* ==========================================================================
 * kputs — String yaz
 * ========================================================================== */
void kputs(const char *s)
{
    while (*s) {
        kputchar(*s++);
    }
}

/* ==========================================================================
 * kprintf — Kernel printf
 * ========================================================================== */
int kprintf(const char *fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    kputs(buf);
    return ret;
}
