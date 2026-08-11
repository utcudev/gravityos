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

void pci_init(void);
void pci_scan_bus(void);

#endif /* DRIVERS_PCI_H */
