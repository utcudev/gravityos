/* =============================================================================
 * GravityOS — Intel E1000 (82540EM) Ethernet Sürücüsü
 * =============================================================================
 * QEMU'nun varsayılan ağ kartı. Kesme kullanmaz; alım halkası yoklanır (polling).
 * ============================================================================= */

#ifndef DRIVERS_E1000_H
#define DRIVERS_E1000_H

#include <stdint.h>

#define ETH_MTU        1500
#define ETH_FRAME_MAX  1518
#define MAC_LEN        6

/* Kartı bul, kur ve alım/gönderim halkalarını hazırla. Başarıda 1. */
int e1000_init(void);
int e1000_present(void);

/* Kartın MAC adresi (6 bayt) */
const uint8_t *e1000_mac(void);

/* Çerçeve gönder. Başarıda 0. */
int e1000_send(const void *frame, uint16_t length);

/* Bekleyen çerçeve varsa buffer'a kopyalar ve uzunluğunu döner; yoksa 0. */
uint16_t e1000_receive(void *buffer, uint16_t buffer_size);

#endif /* DRIVERS_E1000_H */
