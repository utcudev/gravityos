/* =============================================================================
 * GravityOS — PS/2 Mouse Driver Header
 * ============================================================================= */

#ifndef DRIVERS_MOUSE_H
#define DRIVERS_MOUSE_H

#include <stdint.h>

void mouse_init(void);

/* Mouse cursor koordinatları */
extern int32_t mouse_x;
extern int32_t mouse_y;
extern uint8_t mouse_left_pressed;
extern uint8_t mouse_right_pressed;

#endif /* DRIVERS_MOUSE_H */
