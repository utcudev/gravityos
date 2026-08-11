/* =============================================================================
 * GravityOS — PCI (Peripheral Component Interconnect) Driver
 * =============================================================================
 * Anakart üzerindeki donanımları (Ekran kartı, Ethernet vs.) tespit eder.
 * ============================================================================= */

#ifndef DRIVERS_PCI_H
#define DRIVERS_PCI_H

#include <stdint.h>

/* PCI Configuration Space Port Adresleri */
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

/* Bulunan bir PCI cihazının adresi */
typedef struct {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
} pci_device_t;

void pci_init(void);
void pci_scan_bus(void);

uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

/* Vendor/device kimliğine göre cihaz ara. Bulursa 1 döner. */
int pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_t *out);

/* BAR (Base Address Register) değerini oku — MMIO taban adresi için */
uint64_t pci_get_bar(const pci_device_t *dev, int bar_index);

/* Cihazın bus master (DMA) yetkisini aç */
void pci_enable_bus_master(const pci_device_t *dev);

#endif /* DRIVERS_PCI_H */
