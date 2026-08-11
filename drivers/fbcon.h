/* =============================================================================
 * GravityOS — Framebuffer Console (fbcon)
 * =============================================================================
 * Grafik modda metin konsolu. Ekranın belirli bir dikdörtgenine yazar,
 * satır sonunda kaydırma (scroll) yapar. kprintf çıktısı buraya düşer.
 * ============================================================================= */

#ifndef DRIVERS_FBCON_H
#define DRIVERS_FBCON_H

#include <stdint.h>
#include "fb.h"

/* Karakter hücresi boyutu (8x8 font + 2 piksel satır arası) */
#define FBCON_CELL_W 8
#define FBCON_CELL_H 10

/* Konsol renkleri */
#define FBC_WHITE   FB_COLOR(220, 220, 220)
#define FBC_GREY    FB_COLOR(160, 160, 160)
#define FBC_GREEN   FB_COLOR( 80, 220,  90)
#define FBC_CYAN    FB_COLOR( 80, 200, 220)
#define FBC_BLUE    FB_COLOR(100, 150, 255)
#define FBC_YELLOW  FB_COLOR(230, 200,  90)
#define FBC_RED     FB_COLOR(230,  90,  90)

/* Konsolu ekranın (x, y) noktasında w x h piksellik alana kur */
void fbcon_init(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t bg_color);

void fbcon_putchar(char c);

/* Yanıp sönen imleç: periyodik olarak (örn. yarım saniyede bir) çağrılır */
void fbcon_cursor_blink(void);
void fbcon_clear(void);
void fbcon_set_color(uint32_t fg_color);
int  fbcon_ready(void);

#endif /* DRIVERS_FBCON_H */
