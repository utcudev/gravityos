/* =============================================================================
 * GravityOS — Framebuffer Driver Header (Custom Bootloader)
 * ============================================================================= */

#ifndef DRIVERS_FB_H
#define DRIVERS_FB_H

#include <stdint.h>
#include "../kernel/kernel.h"

#define FB_COLOR(r, g, b) (((r) << 16) | ((g) << 8) | (b))

void fb_init(boot_info_t *info);
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_getpixel(uint32_t x, uint32_t y);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void fb_clear(uint32_t color);

uint32_t fb_get_width(void);
uint32_t fb_get_height(void);

#endif /* DRIVERS_FB_H */
