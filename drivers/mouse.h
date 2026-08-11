/* =============================================================================
 * GravityOS — PS/2 Mouse Driver Header
 * ============================================================================= */

#ifndef DRIVERS_MOUSE_H
#define DRIVERS_MOUSE_H

#include <stdint.h>

void mouse_init(void);

/* PS/2 tamponundan gelen bir fare baytını işle.
   Hem IRQ 12 hem de IRQ 1 handler'ı buraya yönlendirebilir. */
void mouse_handle_byte(uint8_t data);

/* Mouse cursor koordinatları */
extern int32_t mouse_x;
extern int32_t mouse_y;
extern uint8_t mouse_left_pressed;
extern uint8_t mouse_right_pressed;

#endif /* DRIVERS_MOUSE_H */
