/* =============================================================================
 * GravityOS — I/O Port Access
 * =============================================================================
 * x86 port I/O fonksiyonları (inline assembly)
 * ============================================================================= */

#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

/* Bir byte oku (port'tan) */
static inline uint8_t inb(uint16_t port)
{
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* Bir byte yaz (port'a) */
static inline void outb(uint16_t port, uint8_t data)
{
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

/* Bir word (16-bit) oku */
static inline uint16_t inw(uint16_t port)
{
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* Bir word (16-bit) yaz */
static inline void outw(uint16_t port, uint16_t data)
{
    __asm__ volatile("outw %0, %1" : : "a"(data), "Nd"(port));
}

/* Bir dword (32-bit) oku */
static inline uint32_t inl(uint16_t port)
{
    uint32_t result;
    __asm__ volatile("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* Bir dword (32-bit) yaz */
static inline void outl(uint16_t port, uint32_t data)
{
    __asm__ volatile("outl %0, %1" : : "a"(data), "Nd"(port));
}

/* Kısa gecikme (I/O port'a yazarak) */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif /* PORTS_H */
