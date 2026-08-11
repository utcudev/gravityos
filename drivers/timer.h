/* =============================================================================
 * GravityOS — PIT Timer Driver Header
 * ============================================================================= */

#ifndef DRIVERS_TIMER_H
#define DRIVERS_TIMER_H

#include <stdint.h>

/* PIT portları */
#define PIT_CHANNEL0 0x40
#define PIT_CMD      0x43

/* PIT base frekansı */
#define PIT_BASE_FREQ 1193182

/* Fonksiyonlar */
void     timer_init(uint32_t frequency);
uint64_t timer_get_ticks(void);
void     sleep_ms(uint32_t ms);

#endif /* DRIVERS_TIMER_H */
