/* =============================================================================
 * GravityOS — PIT Timer Driver
 * =============================================================================
 * Programmable Interval Timer (8254) sürücüsü
 * IRQ 0 ile periyodik tick üretir
 * ============================================================================= */

#include "timer.h"
#include "../cpu/idt.h"
#include "../cpu/ports.h"
#include "../kernel/process.h"

/* Timer durumu */
static volatile uint64_t timer_ticks = 0;
static uint32_t timer_frequency = 0;

/* ==========================================================================
 * timer_irq_handler — IRQ 0 handler (timer tick)
 * ========================================================================== */
static void timer_irq_handler(cpu_state_t *regs)
{
    (void)regs;
    timer_ticks++;

    /* Zamanlayıcı her tetiklendiğinde çalışan süreci değiştir (Multitasking) */
    process_schedule(regs);
}

/* ==========================================================================
 * timer_init — PIT'i belirtilen frekansa ayarla
 * ========================================================================== */
void timer_init(uint32_t frequency)
{
    timer_frequency = frequency;

    /* Divisor hesapla */
    uint16_t divisor = (uint16_t)(PIT_BASE_FREQ / frequency);

    /* PIT Channel 0, Square Wave, Access mode lo/hi */
    outb(PIT_CMD, 0x36);

    /* Divisor'u gönder (low byte first, then high byte) */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    /* IRQ 0 handler'ını kaydet */
    irq_install_handler(0, timer_irq_handler);
}

/* ==========================================================================
 * timer_get_ticks — Geçen toplam tick sayısı
 * ========================================================================== */
uint64_t timer_get_ticks(void)
{
    return timer_ticks;
}

/* ==========================================================================
 * sleep_ms — Milisaniye cinsinden bekle
 * ========================================================================== */
void sleep_ms(uint32_t ms)
{
    uint64_t target = timer_ticks + (ms * timer_frequency) / 1000;
    while (timer_ticks < target) {
        __asm__ volatile("hlt"); /* Tick bekle */
    }
}
