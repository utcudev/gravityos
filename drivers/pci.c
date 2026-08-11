/* =============================================================================
 * GravityOS — PCI Driver Implementation
 * ============================================================================= */

#include "pci.h"
#include "../lib/stdio.h"
#include "../cpu/ports.h" /* For inb, outb, outl, inl */

/* Cihaz isimlerini tanımlayan ufak bir veritabanı (Mock) */
const char* pci_get_device_name(uint16_t vendor_id, uint16_t device_id)
{
    if (vendor_id == 0x8086) return "Intel Corporation Device";
    if (vendor_id == 0x10EC) return "Realtek Semiconductor";
    if (vendor_id == 0x10DE) return "NVIDIA Corporation";
    if (vendor_id == 0x1022) return "Advanced Micro Devices [AMD]";
    if (vendor_id == 0x15AD) return "VMware Inc.";
    if (vendor_id == 0x1234) return "QEMU Virtual Video Controller";
    if (vendor_id == 0x106B) return "Apple Inc.";
    return "Unknown Device";
}

/* PCI Configuration Space Okuma */
uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    
    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value)
{
    uint32_t address = (uint32_t)(((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                                  ((uint32_t)func << 8) | (offset & 0xFC) | 0x80000000);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_t *out)
{
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id_reg = pci_config_read_dword((uint8_t)bus, slot, 0, 0);
            if ((uint16_t)(id_reg & 0xFFFF) != vendor_id) continue;
            if ((uint16_t)(id_reg >> 16) != device_id) continue;

            out->bus       = (uint8_t)bus;
            out->slot      = slot;
            out->func      = 0;
            out->vendor_id = vendor_id;
            out->device_id = device_id;
            return 1;
        }
    }
    return 0;
}

uint64_t pci_get_bar(const pci_device_t *dev, int bar_index)
{
    uint8_t offset = (uint8_t)(0x10 + bar_index * 4);
    uint32_t bar = pci_config_read_dword(dev->bus, dev->slot, dev->func, offset);

    /* Bit 0: 1 ise G/Ç portu, 0 ise bellek adresi */
    if (bar & 1) {
        return bar & ~0x3ULL;
    }

    /* Bit 2-1 = 10 ise 64-bit BAR; üst yarısı bir sonraki BAR'da */
    if (((bar >> 1) & 3) == 2) {
        uint32_t high = pci_config_read_dword(dev->bus, dev->slot, dev->func, (uint8_t)(offset + 4));
        return ((uint64_t)high << 32) | (bar & ~0xFULL);
    }

    return bar & ~0xFULL;
}

void pci_enable_bus_master(const pci_device_t *dev)
{
    uint32_t cmd = pci_config_read_dword(dev->bus, dev->slot, dev->func, 0x04);
    /* Bit 0: G/Ç alanı, bit 1: bellek alanı, bit 2: bus master (DMA) */
    cmd |= (1 << 0) | (1 << 1) | (1 << 2);
    pci_config_write_dword(dev->bus, dev->slot, dev->func, 0x04, cmd);
}

void pci_scan_bus(void)
{
    kprintf("[PCI] Scanning Motherboard for Devices...\n");
    
    int device_count = 0;
    
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            /* Vendor ID oku (Offset 0) */
            uint32_t id_reg = pci_config_read_dword(bus, slot, 0, 0);
            uint16_t vendor_id = (uint16_t)(id_reg & 0xFFFF);
            uint16_t device_id = (uint16_t)(id_reg >> 16);
            
            if (vendor_id == 0xFFFF) {
                continue; /* Bu slot boş */
            }
            
            /* Class Code ve Subclass oku (Offset 8) */
            uint32_t class_reg = pci_config_read_dword(bus, slot, 0, 8);
            uint8_t class_code = (uint8_t)(class_reg >> 24);
            uint8_t subclass = (uint8_t)(class_reg >> 16);
            
            const char* dev_name = pci_get_device_name(vendor_id, device_id);
            
            kprintf("[PCI] Found: Bus %d Slot %d -> Vendor: 0x%x, Device: 0x%x\n", bus, slot, vendor_id, device_id);
            kprintf("      -> Class: %d Subclass: %d (%s)\n", class_code, subclass, dev_name);
            
            device_count++;
        }
    }
    
    kprintf("[PCI] Scan Complete. Total %d devices found.\n", device_count);
}

void pci_init(void)
{
    pci_scan_bus();
}
