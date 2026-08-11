/* =============================================================================
 * GravityOS — Intel E1000 (82540EM) Ethernet Sürücüsü
 * =============================================================================
 * Kart, alım ve gönderim için bellekteki tanımlayıcı halkalarını (descriptor
 * ring) DMA ile okur. Bizim işimiz halkaları kurmak, kartın kuyruk (tail)
 * göstergesini ilerletmek ve dolan tanımlayıcıları toplamak.
 * ============================================================================= */

#include "e1000.h"
#include "pci.h"
#include "../kernel/pmm.h"
#include "../kernel/vmm.h"
#include "../lib/stdio.h"
#include "../lib/string.h"

#define E1000_VENDOR 0x8086
#define E1000_DEVICE 0x100E

/* Register offsetleri */
#define REG_CTRL     0x0000
#define REG_STATUS   0x0008
#define REG_EERD     0x0014
#define REG_ICR      0x00C0
#define REG_IMC      0x00D8
#define REG_RCTL     0x0100
#define REG_TCTL     0x0400
#define REG_TIPG     0x0410
#define REG_RDBAL    0x2800
#define REG_RDBAH    0x2804
#define REG_RDLEN    0x2808
#define REG_RDH      0x2810
#define REG_RDT      0x2818
#define REG_TDBAL    0x3800
#define REG_TDBAH    0x3804
#define REG_TDLEN    0x3808
#define REG_TDH      0x3810
#define REG_TDT      0x3818
#define REG_MTA      0x5200
#define REG_RAL      0x5400
#define REG_RAH      0x5404

/* CTRL bitleri */
#define CTRL_SLU     (1 << 6)   /* Set Link Up */
#define CTRL_RST     (1 << 26)

/* RCTL bitleri */
#define RCTL_EN      (1 << 1)
#define RCTL_BAM     (1 << 15)  /* Broadcast kabul et */
#define RCTL_SECRC   (1 << 26)  /* Ethernet CRC'sini kırp */

/* TCTL bitleri */
#define TCTL_EN      (1 << 1)
#define TCTL_PSP     (1 << 3)   /* Kısa paketleri doldur */

/* TX komut bitleri */
#define TXD_CMD_EOP  (1 << 0)
#define TXD_CMD_IFCS (1 << 1)
#define TXD_CMD_RS   (1 << 3)
#define TXD_STAT_DD  (1 << 0)

#define RXD_STAT_DD  (1 << 0)
#define RXD_STAT_EOP (1 << 1)

#define NUM_RX_DESC  16
#define NUM_TX_DESC  8
#define RX_BUF_SIZE  2048

/* MMIO penceresi için kullanacağımız sanal adres */
#define E1000_MMIO_VIRT 0xFFFFFFFFC0000000ULL
#define E1000_MMIO_SIZE 0x20000   /* 128 KB */

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} rx_desc_t;

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} tx_desc_t;

extern uint64_t hhdm_offset;

static int      card_present = 0;
static volatile uint8_t *mmio = NULL;
static uint8_t  mac_addr[MAC_LEN];

static volatile rx_desc_t *rx_ring = NULL;
static volatile tx_desc_t *tx_ring = NULL;
static uint64_t rx_ring_phys = 0;
static uint64_t tx_ring_phys = 0;

static uint8_t *rx_buffers[NUM_RX_DESC];
static uint8_t *tx_buffers[NUM_TX_DESC];
static uint64_t tx_buffers_phys[NUM_TX_DESC];

static uint16_t rx_cur = 0;
static uint16_t tx_cur = 0;

int e1000_present(void)        { return card_present; }
const uint8_t *e1000_mac(void) { return mac_addr; }

static void mmio_write(uint32_t reg, uint32_t value)
{
    *(volatile uint32_t *)(mmio + reg) = value;
}

static uint32_t mmio_read(uint32_t reg)
{
    return *(volatile uint32_t *)(mmio + reg);
}

/* Fiziksel sayfayı kernel'in erişebileceği sanal adrese çevir (HHDM) */
static void *phys_to_virt(uint64_t phys)
{
    return (void *)(phys + hhdm_offset);
}

/* MAC adresini önce RAL/RAH'tan, olmazsa EEPROM'dan oku */
static void read_mac(void)
{
    uint32_t ral = mmio_read(REG_RAL);
    uint32_t rah = mmio_read(REG_RAH);

    if (ral != 0) {
        mac_addr[0] = (uint8_t)(ral);
        mac_addr[1] = (uint8_t)(ral >> 8);
        mac_addr[2] = (uint8_t)(ral >> 16);
        mac_addr[3] = (uint8_t)(ral >> 24);
        mac_addr[4] = (uint8_t)(rah);
        mac_addr[5] = (uint8_t)(rah >> 8);
        return;
    }

    /* EEPROM yolu: her okuma 16 bit verir */
    for (int i = 0; i < 3; i++) {
        mmio_write(REG_EERD, (uint32_t)(i << 8) | 1);

        uint32_t value = 0;
        for (int spin = 0; spin < 1000000; spin++) {
            value = mmio_read(REG_EERD);
            if (value & (1 << 4)) break;
        }

        mac_addr[i * 2]     = (uint8_t)(value >> 16);
        mac_addr[i * 2 + 1] = (uint8_t)(value >> 24);
    }
}

static int setup_rx(void)
{
    void *ring_phys = pmm_alloc_page();
    if (!ring_phys) return 0;

    rx_ring_phys = (uint64_t)ring_phys;
    rx_ring = (volatile rx_desc_t *)phys_to_virt(rx_ring_phys);
    memset((void *)rx_ring, 0, PMM_PAGE_SIZE);

    for (int i = 0; i < NUM_RX_DESC; i++) {
        void *buf_phys = pmm_alloc_page();
        if (!buf_phys) return 0;

        rx_buffers[i]     = (uint8_t *)phys_to_virt((uint64_t)buf_phys);
        rx_ring[i].addr   = (uint64_t)buf_phys;
        rx_ring[i].status = 0;
    }

    mmio_write(REG_RDBAL, (uint32_t)(rx_ring_phys & 0xFFFFFFFF));
    mmio_write(REG_RDBAH, (uint32_t)(rx_ring_phys >> 32));
    mmio_write(REG_RDLEN, NUM_RX_DESC * sizeof(rx_desc_t));
    mmio_write(REG_RDH, 0);
    mmio_write(REG_RDT, NUM_RX_DESC - 1);

    rx_cur = 0;
    mmio_write(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC);
    return 1;
}

static int setup_tx(void)
{
    void *ring_phys = pmm_alloc_page();
    if (!ring_phys) return 0;

    tx_ring_phys = (uint64_t)ring_phys;
    tx_ring = (volatile tx_desc_t *)phys_to_virt(tx_ring_phys);
    memset((void *)tx_ring, 0, PMM_PAGE_SIZE);

    for (int i = 0; i < NUM_TX_DESC; i++) {
        void *buf_phys = pmm_alloc_page();
        if (!buf_phys) return 0;

        tx_buffers_phys[i]  = (uint64_t)buf_phys;
        tx_buffers[i]       = (uint8_t *)phys_to_virt((uint64_t)buf_phys);
        tx_ring[i].addr     = 0;
        tx_ring[i].status   = TXD_STAT_DD; /* boş sayılsın */
    }

    mmio_write(REG_TDBAL, (uint32_t)(tx_ring_phys & 0xFFFFFFFF));
    mmio_write(REG_TDBAH, (uint32_t)(tx_ring_phys >> 32));
    mmio_write(REG_TDLEN, NUM_TX_DESC * sizeof(tx_desc_t));
    mmio_write(REG_TDH, 0);
    mmio_write(REG_TDT, 0);

    tx_cur = 0;
    mmio_write(REG_TCTL, TCTL_EN | TCTL_PSP | (0x10 << 4) | (0x40 << 12));
    mmio_write(REG_TIPG, 10 | (8 << 10) | (6 << 20));
    return 1;
}

int e1000_init(void)
{
    card_present = 0;

    pci_device_t dev;
    if (!pci_find_device(E1000_VENDOR, E1000_DEVICE, &dev)) {
        kprintf("[E1000] No Intel 82540EM network card found.\n");
        return 0;
    }

    uint64_t bar0 = pci_get_bar(&dev, 0);
    if (bar0 == 0) {
        kprintf("[E1000] BAR0 is empty.\n");
        return 0;
    }

    pci_enable_bus_master(&dev);

    /* MMIO penceresini haritala — önbelleğe alınmamalı */
    for (uint64_t off = 0; off < E1000_MMIO_SIZE; off += PMM_PAGE_SIZE) {
        vmm_map_page(bar0 + off, E1000_MMIO_VIRT + off,
                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_NOCACHE);
    }
    mmio = (volatile uint8_t *)E1000_MMIO_VIRT;

    /* Kesmeleri kapat — yoklama (polling) ile çalışıyoruz */
    mmio_write(REG_IMC, 0xFFFFFFFF);
    mmio_read(REG_ICR);

    /* Bağlantıyı yukarı al */
    mmio_write(REG_CTRL, mmio_read(REG_CTRL) | CTRL_SLU);

    /* Çok noktaya yayın tablosunu temizle */
    for (int i = 0; i < 128; i++) {
        mmio_write(REG_MTA + i * 4, 0);
    }

    read_mac();

    if (!setup_rx() || !setup_tx()) {
        kprintf("[E1000] Ring setup failed (out of memory).\n");
        return 0;
    }

    card_present = 1;
    kprintf("[E1000] NIC ready, MAC %02x:%02x:%02x:%02x:%02x:%02x, link %s\n",
            mac_addr[0], mac_addr[1], mac_addr[2],
            mac_addr[3], mac_addr[4], mac_addr[5],
            (mmio_read(REG_STATUS) & 2) ? "up" : "down");
    return 1;
}

int e1000_send(const void *frame, uint16_t length)
{
    if (!card_present) return -1;
    if (length == 0 || length > ETH_FRAME_MAX) return -1;

    /* Sıradaki tanımlayıcının boşalmasını bekle */
    for (int spin = 0; spin < 1000000; spin++) {
        if (tx_ring[tx_cur].status & TXD_STAT_DD) break;
    }
    if (!(tx_ring[tx_cur].status & TXD_STAT_DD)) {
        kprintf("[E1000] TX ring stuck.\n");
        return -1;
    }

    memcpy(tx_buffers[tx_cur], frame, length);

    tx_ring[tx_cur].addr   = tx_buffers_phys[tx_cur];
    tx_ring[tx_cur].length = length;
    tx_ring[tx_cur].cso    = 0;
    tx_ring[tx_cur].css    = 0;
    tx_ring[tx_cur].status = 0;
    tx_ring[tx_cur].cmd    = TXD_CMD_EOP | TXD_CMD_IFCS | TXD_CMD_RS;

    tx_cur = (uint16_t)((tx_cur + 1) % NUM_TX_DESC);
    mmio_write(REG_TDT, tx_cur);

    return 0;
}

uint16_t e1000_receive(void *buffer, uint16_t buffer_size)
{
    if (!card_present) return 0;

    if (!(rx_ring[rx_cur].status & RXD_STAT_DD)) return 0;

    uint16_t length = rx_ring[rx_cur].length;
    if (length > buffer_size) length = buffer_size;

    memcpy(buffer, rx_buffers[rx_cur], length);

    rx_ring[rx_cur].status = 0;

    /* Tanımlayıcıyı karta geri ver: tail bir önceki girdiyi göstermeli */
    mmio_write(REG_RDT, rx_cur);
    rx_cur = (uint16_t)((rx_cur + 1) % NUM_RX_DESC);

    return length;
}
