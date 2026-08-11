/* =============================================================================
 * GravityOS — Ağ Yığını (Ethernet / ARP / IPv4 / ICMP)
 * =============================================================================
 * QEMU'nun user-mode ağında varsayılan yerleşim:
 *   misafir 10.0.2.15, ağ geçidi 10.0.2.2, DNS 10.0.2.3
 * ============================================================================= */

#ifndef NET_NET_H
#define NET_NET_H

#include <stdint.h>

#define IPV4_ADDR(a, b, c, d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

/* Ağ yığınını başlat (kart hazır olmalı) */
void net_init(void);
int  net_ready(void);

/* Gelen çerçeveleri işle — ağ sürecinden sürekli çağrılır */
void net_poll(void);

/* Yapılandırma */
uint32_t net_local_ip(void);
uint32_t net_gateway_ip(void);
void     net_set_ip(uint32_t ip, uint32_t netmask, uint32_t gateway);

/* ARP: hedefin MAC adresini çöz. Başarıda 1, zaman aşımında 0. */
int net_arp_resolve(uint32_t ip, uint8_t mac_out[6]);

/* ICMP echo gönder ve yanıtı bekle. Dönüş: gidiş-dönüş süresi (tick),
   yanıt gelmezse -1. */
int net_ping(uint32_t ip, uint16_t sequence);

/* İstatistikler */
uint32_t net_rx_count(void);
uint32_t net_tx_count(void);

#endif /* NET_NET_H */
