/* =============================================================================
 * GravityOS — Ağ Yığını (Ethernet / ARP / IPv4 / ICMP)
 * =============================================================================
 * Ağ baytları büyük uçlu (big-endian), x86 küçük uçlu (little-endian) olduğu
 * için başlıklardaki her çok baytlı alan htons/htonl ile çevrilir. Bu adımın
 * atlanması ağ kodundaki en yaygın hata kaynağıdır.
 * ============================================================================= */

#include "net.h"
#include "../drivers/e1000.h"
#include "../drivers/timer.h"
#include "../lib/stdio.h"
#include "../lib/string.h"

#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP  0x0806

#define ARP_REQUEST 1
#define ARP_REPLY   2

#define IP_PROTO_ICMP 1

#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8

#define ARP_CACHE_SIZE 8

typedef struct __attribute__((packed)) {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t type;
} eth_hdr_t;

typedef struct __attribute__((packed)) {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} arp_packet_t;

typedef struct __attribute__((packed)) {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} ipv4_hdr_t;

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} icmp_hdr_t;

typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    int      valid;
} arp_entry_t;

static int      stack_ready = 0;
static uint32_t local_ip    = IPV4_ADDR(10, 0, 2, 15);
static uint32_t netmask     = IPV4_ADDR(255, 255, 255, 0);
static uint32_t gateway_ip  = IPV4_ADDR(10, 0, 2, 2);

static arp_entry_t arp_cache[ARP_CACHE_SIZE];

static uint32_t rx_count = 0;
static uint32_t tx_count = 0;

/* Bekleyen ping durumu — net_poll yanıtı görünce doldurur */
static volatile uint16_t ping_wait_seq   = 0;
static volatile int      ping_wait_active = 0;
static volatile int      ping_got_reply   = 0;

static uint8_t rx_frame[ETH_FRAME_MAX];
static uint8_t tx_frame[ETH_FRAME_MAX];

static const uint8_t broadcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

int      net_ready(void)      { return stack_ready; }
uint32_t net_local_ip(void)   { return local_ip; }
uint32_t net_gateway_ip(void) { return gateway_ip; }
uint32_t net_rx_count(void)   { return rx_count; }
uint32_t net_tx_count(void)   { return tx_count; }

void net_set_ip(uint32_t ip, uint32_t mask, uint32_t gw)
{
    local_ip   = ip;
    netmask    = mask;
    gateway_ip = gw;
}

/* --- Bayt sırası dönüşümleri ------------------------------------------- */

static uint16_t htons(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint32_t htonl(uint32_t value)
{
    return ((value & 0x000000FF) << 24) |
           ((value & 0x0000FF00) << 8)  |
           ((value & 0x00FF0000) >> 8)  |
           ((value & 0xFF000000) >> 24);
}

#define ntohs htons
#define ntohl htonl

/* --- İnternet sağlama toplamı (RFC 1071) -------------------------------- */

static uint16_t checksum16(const void *data, uint32_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t sum = 0;

    while (length > 1) {
        sum += ((uint32_t)bytes[0] << 8) | bytes[1];
        bytes += 2;
        length -= 2;
    }
    if (length == 1) sum += (uint32_t)bytes[0] << 8;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)~sum;
}

/* --- ARP önbelleği ------------------------------------------------------ */

static void arp_cache_put(uint32_t ip, const uint8_t mac[6])
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(arp_cache[i].mac, mac, 6);
            return;
        }
    }
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            arp_cache[i].ip = ip;
            memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = 1;
            return;
        }
    }
    /* Dolu: ilk girdiyi devir */
    arp_cache[0].ip = ip;
    memcpy(arp_cache[0].mac, mac, 6);
}

static int arp_cache_get(uint32_t ip, uint8_t mac_out[6])
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(mac_out, arp_cache[i].mac, 6);
            return 1;
        }
    }
    return 0;
}

/* --- Çerçeve gönderimi -------------------------------------------------- */

static int send_frame(const uint8_t dst_mac[6], uint16_t ether_type,
                      const void *payload, uint16_t payload_len)
{
    if (payload_len + sizeof(eth_hdr_t) > ETH_FRAME_MAX) return -1;

    eth_hdr_t *eth = (eth_hdr_t *)tx_frame;
    memcpy(eth->dst, dst_mac, 6);
    memcpy(eth->src, e1000_mac(), 6);
    eth->type = htons(ether_type);

    memcpy(tx_frame + sizeof(eth_hdr_t), payload, payload_len);

    uint16_t total = (uint16_t)(sizeof(eth_hdr_t) + payload_len);
    /* Ethernet en az 60 bayt ister (CRC hariç) */
    if (total < 60) {
        memset(tx_frame + total, 0, 60 - total);
        total = 60;
    }

    int rc = e1000_send(tx_frame, total);
    if (rc == 0) tx_count++;
    return rc;
}

static void send_arp(uint16_t opcode, const uint8_t target_mac[6], uint32_t target_ip)
{
    arp_packet_t arp;
    arp.hw_type    = htons(1);              /* Ethernet */
    arp.proto_type = htons(ETH_TYPE_IPV4);
    arp.hw_len     = 6;
    arp.proto_len  = 4;
    arp.opcode     = htons(opcode);
    memcpy(arp.sender_mac, e1000_mac(), 6);
    arp.sender_ip  = htonl(local_ip);
    memcpy(arp.target_mac, target_mac, 6);
    arp.target_ip  = htonl(target_ip);

    send_frame(opcode == ARP_REQUEST ? broadcast_mac : target_mac,
               ETH_TYPE_ARP, &arp, sizeof(arp));
}

static int send_ipv4(uint32_t dst_ip, uint8_t protocol,
                     const void *payload, uint16_t payload_len)
{
    uint8_t packet[ETH_MTU];
    if (sizeof(ipv4_hdr_t) + payload_len > sizeof(packet)) return -1;

    ipv4_hdr_t *ip = (ipv4_hdr_t *)packet;
    ip->version_ihl    = 0x45;  /* IPv4, 5 kelimelik başlık */
    ip->tos            = 0;
    ip->total_length   = htons((uint16_t)(sizeof(ipv4_hdr_t) + payload_len));
    ip->id             = htons(0);
    ip->flags_fragment = htons(0x4000); /* Don't Fragment */
    ip->ttl            = 64;
    ip->protocol       = protocol;
    ip->checksum       = 0;
    ip->src_ip         = htonl(local_ip);
    ip->dst_ip         = htonl(dst_ip);
    ip->checksum       = htons(checksum16(ip, sizeof(ipv4_hdr_t)));

    memcpy(packet + sizeof(ipv4_hdr_t), payload, payload_len);

    /* Hedef ağ dışındaysa ağ geçidinin MAC'ine gönder */
    uint32_t next_hop = ((dst_ip & netmask) == (local_ip & netmask)) ? dst_ip : gateway_ip;

    uint8_t dst_mac[6];
    if (!net_arp_resolve(next_hop, dst_mac)) return -1;

    return send_frame(dst_mac, ETH_TYPE_IPV4, packet,
                      (uint16_t)(sizeof(ipv4_hdr_t) + payload_len));
}

/* --- Gelen paketlerin işlenmesi ----------------------------------------- */

static void handle_arp(const uint8_t *payload, uint16_t length)
{
    if (length < sizeof(arp_packet_t)) return;

    const arp_packet_t *arp = (const arp_packet_t *)payload;
    uint32_t sender_ip = ntohl(arp->sender_ip);
    uint32_t target_ip = ntohl(arp->target_ip);

    arp_cache_put(sender_ip, arp->sender_mac);

    /* Bize sorulduysa cevap ver */
    if (ntohs(arp->opcode) == ARP_REQUEST && target_ip == local_ip) {
        send_arp(ARP_REPLY, arp->sender_mac, sender_ip);
    }
}

static void handle_icmp(const ipv4_hdr_t *ip, const uint8_t *payload, uint16_t length)
{
    if (length < sizeof(icmp_hdr_t)) return;

    const icmp_hdr_t *icmp = (const icmp_hdr_t *)payload;

    if (icmp->type == ICMP_ECHO_REPLY) {
        if (ping_wait_active && ntohs(icmp->sequence) == ping_wait_seq) {
            ping_got_reply = 1;
        }
        return;
    }

    if (icmp->type == ICMP_ECHO_REQUEST) {
        /* Aynı gövdeyi echo reply olarak geri yolla */
        uint8_t reply[ETH_MTU];
        if (length > sizeof(reply)) return;

        memcpy(reply, payload, length);
        icmp_hdr_t *out = (icmp_hdr_t *)reply;
        out->type     = ICMP_ECHO_REPLY;
        out->checksum = 0;
        out->checksum = htons(checksum16(reply, length));

        send_ipv4(ntohl(ip->src_ip), IP_PROTO_ICMP, reply, length);
    }
}

static void handle_ipv4(const uint8_t *payload, uint16_t length)
{
    if (length < sizeof(ipv4_hdr_t)) return;

    const ipv4_hdr_t *ip = (const ipv4_hdr_t *)payload;

    if ((ip->version_ihl >> 4) != 4) return;

    uint16_t header_len = (uint16_t)((ip->version_ihl & 0x0F) * 4);
    if (header_len < sizeof(ipv4_hdr_t) || header_len > length) return;

    if (ntohl(ip->dst_ip) != local_ip) return; /* bize değil */

    uint16_t total_len = ntohs(ip->total_length);
    if (total_len > length) total_len = length;

    const uint8_t *inner = payload + header_len;
    uint16_t inner_len = (uint16_t)(total_len - header_len);

    if (ip->protocol == IP_PROTO_ICMP) {
        handle_icmp(ip, inner, inner_len);
    }
}

void net_poll(void)
{
    if (!stack_ready) return;

    uint16_t length = e1000_receive(rx_frame, sizeof(rx_frame));
    while (length > 0) {
        rx_count++;

        if (length >= sizeof(eth_hdr_t)) {
            const eth_hdr_t *eth = (const eth_hdr_t *)rx_frame;
            const uint8_t *payload = rx_frame + sizeof(eth_hdr_t);
            uint16_t payload_len = (uint16_t)(length - sizeof(eth_hdr_t));

            switch (ntohs(eth->type)) {
            case ETH_TYPE_ARP:  handle_arp(payload, payload_len);  break;
            case ETH_TYPE_IPV4: handle_ipv4(payload, payload_len); break;
            default: break;
            }
        }

        length = e1000_receive(rx_frame, sizeof(rx_frame));
    }
}

/* --- Dışa açık işlemler ------------------------------------------------- */

int net_arp_resolve(uint32_t ip, uint8_t mac_out[6])
{
    if (arp_cache_get(ip, mac_out)) return 1;

    /* Üç deneme, her biri ~200 ms */
    for (int attempt = 0; attempt < 3; attempt++) {
        send_arp(ARP_REQUEST, broadcast_mac, ip);

        uint64_t deadline = timer_get_ticks() + 20;
        while (timer_get_ticks() < deadline) {
            net_poll();
            if (arp_cache_get(ip, mac_out)) return 1;
            __asm__ volatile("pause");
        }
    }

    return 0;
}

int net_ping(uint32_t ip, uint16_t sequence)
{
    if (!stack_ready) return -1;

    uint8_t packet[64];
    memset(packet, 0, sizeof(packet));

    icmp_hdr_t *icmp = (icmp_hdr_t *)packet;
    icmp->type     = ICMP_ECHO_REQUEST;
    icmp->code     = 0;
    icmp->id       = htons(0x4756); /* 'GV' */
    icmp->sequence = htons(sequence);

    /* Gövdeye tanınabilir bir desen koy */
    for (unsigned i = sizeof(icmp_hdr_t); i < sizeof(packet); i++) {
        packet[i] = (uint8_t)('a' + (i % 26));
    }

    icmp->checksum = 0;
    icmp->checksum = htons(checksum16(packet, sizeof(packet)));

    ping_wait_seq    = sequence;
    ping_got_reply   = 0;
    ping_wait_active = 1;

    uint64_t start = timer_get_ticks();

    if (send_ipv4(ip, IP_PROTO_ICMP, packet, sizeof(packet)) != 0) {
        ping_wait_active = 0;
        return -1;
    }

    /* En fazla 1 saniye bekle */
    uint64_t deadline = start + 100;
    while (timer_get_ticks() < deadline) {
        net_poll();
        if (ping_got_reply) {
            ping_wait_active = 0;
            return (int)(timer_get_ticks() - start);
        }
        __asm__ volatile("pause");
    }

    ping_wait_active = 0;
    return -1;
}

void net_init(void)
{
    memset(arp_cache, 0, sizeof(arp_cache));

    if (!e1000_present()) {
        kprintf("[NET] No network card, stack disabled.\n");
        stack_ready = 0;
        return;
    }

    stack_ready = 1;

    kprintf("[NET] IPv4 %u.%u.%u.%u/24, gateway %u.%u.%u.%u\n",
            (local_ip >> 24) & 0xFF, (local_ip >> 16) & 0xFF,
            (local_ip >> 8) & 0xFF, local_ip & 0xFF,
            (gateway_ip >> 24) & 0xFF, (gateway_ip >> 16) & 0xFF,
            (gateway_ip >> 8) & 0xFF, gateway_ip & 0xFF);
}
