/* =============================================================================
 * GravityOS — Serial Port Driver Header
 * ============================================================================= */

#ifndef DRIVERS_SERIAL_H
#define DRIVERS_SERIAL_H

#include <stdint.h>

/* COM port adresleri */
#define COM1 0x3F8
#define COM2 0x2F8

/* Fonksiyonlar */
void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char *s);
int  serial_received(void);
char serial_read(void);

#endif /* DRIVERS_SERIAL_H */
