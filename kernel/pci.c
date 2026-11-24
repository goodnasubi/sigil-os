#include "pci.h"

static inline void outl(uint16_t port, uint32_t val)
{
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t val;
    __asm__ volatile("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

uint32_t pci_read_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) | (offset & 0xfc);
    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_write_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val)
{
    uint32_t address = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) | (offset & 0xfc);
    outl(0xCF8, address);
    outl(0xCFC, val);
}

uint16_t pci_read_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t val = pci_read_config32(bus, slot, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

void pci_write_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val)
{
    uint32_t old = pci_read_config32(bus, slot, func, offset);
    uint32_t shift = (offset & 2) * 8;
    uint32_t mask = 0xFFFFu << shift;
    uint32_t updated = (old & ~mask) | (((uint32_t)val) << shift);
    pci_write_config32(bus, slot, func, offset, updated);
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id, struct pci_device *out_dev)
{
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = pci_read_config32((uint8_t)bus, slot, func, 0);
                uint16_t v = id & 0xFFFF;
                uint16_t d = (id >> 16) & 0xFFFF;
                if (v == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }
                if (v == vendor_id && d == device_id) {
                    if (out_dev) {
                        out_dev->bus = (uint8_t)bus;
                        out_dev->slot = slot;
                        out_dev->func = func;
                        out_dev->vendor_id = v;
                        out_dev->device_id = d;
                        for (uint8_t b = 0; b < 6; ++b) {
                            uint32_t bar_low = pci_read_config32((uint8_t)bus, slot, func, 0x10 + b * 4);
                            if (bar_low & 1) {
                                out_dev->bar_is_io[b] = 1;
                                out_dev->bar[b] = bar_low & ~3u;
                            } else {
                                out_dev->bar_is_io[b] = 0;
                                uint8_t type = (bar_low >> 1) & 3;
                                if (type == 2 && b + 1 < 6) {
                                    uint32_t bar_high = pci_read_config32((uint8_t)bus, slot, func, 0x10 + (b + 1) * 4);
                                    out_dev->bar[b] = ((uint64_t)bar_high << 32) | (bar_low & ~0xFu);
                                    out_dev->bar[b + 1] = 0;
                                    b++;
                                } else {
                                    out_dev->bar[b] = bar_low & ~0xFu;
                                }
                            }
                        }
                    }
                    return 0;
                }
                if (func == 0) {
                    uint32_t header = pci_read_config32((uint8_t)bus, slot, func, 0x0C);
                    if (!((header >> 16) & 0x80))
                        break;
                }
            }
        }
    }
    return -1;
}

void pci_enable_bus_master(const struct pci_device *dev)
{
    uint16_t cmd = pci_read_config16(dev->bus, dev->slot, dev->func, PCI_COMMAND_OFFSET);
    cmd |= (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
    pci_write_config16(dev->bus, dev->slot, dev->func, PCI_COMMAND_OFFSET, cmd);
}
