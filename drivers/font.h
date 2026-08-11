/* =============================================================================
 * GravityOS — Simple 8x8 Bitmap Font
 * ============================================================================= */

#ifndef DRIVERS_FONT_H
#define DRIVERS_FONT_H

#include <stdint.h>
#include "fb.h"

void font_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg_color, uint32_t bg_color);
void font_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg_color, uint32_t bg_color);

#endif /* DRIVERS_FONT_H */
