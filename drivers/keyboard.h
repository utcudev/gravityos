/* =============================================================================
 * GravityOS — Keyboard Driver Header
 * ============================================================================= */

#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <stdint.h>

/* Klavye portları */
#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

/* Klavye buffer boyutu */
#define KB_BUFFER_SIZE 256

/* Fonksiyonlar */
void keyboard_init(void);
char keyboard_getchar(void);     /* Bloklayıcı — tuş basılana kadar bekler */
int  keyboard_has_input(void);   /* Buffer'da karakter var mı? */

#endif /* DRIVERS_KEYBOARD_H */
