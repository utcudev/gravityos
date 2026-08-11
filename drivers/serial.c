/* =============================================================================
 * GravityOS — Serial Port Driver
 * =============================================================================
 * COM1 (0x3F8) serial port sürücüsü
 * Debug çıktısı için kullanılır — QEMU -serial stdio ile terminal çıktısı
 * ============================================================================= */

#include "serial.h"
#include "../cpu/ports.h"

/* ==========================================================================
 * serial_init — COM1'i 115200 baud, 8N1 olarak ayarla
 * ========================================================================== */
void serial_init(void)
{
    outb(COM1 + 1, 0x00);    /* Interrupt'ları kapat */
    outb(COM1 + 3, 0x80);    /* DLAB etkinleştir (baud rate ayarı için) */
    outb(COM1 + 0, 0x01);    /* Baud rate divisor low (115200 baud) */
    outb(COM1 + 1, 0x00);    /* Baud rate divisor high */
    outb(COM1 + 3, 0x03);    /* 8 bit, parity yok, 1 stop bit (8N1) */
    outb(COM1 + 2, 0xC7);    /* FIFO etkinleştir, 14 byte threshold */
    outb(COM1 + 4, 0x0B);    /* IRQ etkin, RTS/DSR set */

    /* Loopback test */
    outb(COM1 + 4, 0x1E);    /* Loopback modu */
    outb(COM1 + 0, 0xAE);    /* Test byte gönder */
    if (inb(COM1 + 0) != 0xAE) {
        /* Serial port arızalı — sessizce devam et */
        return;
    }

    /* Normal çalışma moduna dön */
    outb(COM1 + 4, 0x0F);
}

/* ==========================================================================
 * serial_is_transmit_empty — Gönderme tamponu boş mu?
 * ========================================================================== */
static int serial_is_transmit_empty(void)
{
    return inb(COM1 + 5) & 0x20;
}

/* ==========================================================================
 * serial_putchar — Tek karakter gönder
 * ========================================================================== */
void serial_putchar(char c)
{
    while (!serial_is_transmit_empty())
        ;
    outb(COM1, (uint8_t)c);
}

/* ==========================================================================
 * serial_puts — String gönder
 * ========================================================================== */
void serial_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*s++);
    }
}

/* ==========================================================================
 * serial_received — Veri var mı?
 * ========================================================================== */
int serial_received(void)
{
    return inb(COM1 + 5) & 1;
}

/* ==========================================================================
 * serial_read — Bir byte oku (bloklayıcı)
 * ========================================================================== */
char serial_read(void)
{
    while (!serial_received())
        ;
    return (char)inb(COM1);
}
