#ifndef SIGIL_OS_KERNEL_PCI_H
#define SIGIL_OS_KERNEL_PCI_H

#include <stdint.h>

#define PCI_VENDOR_VMWARE 0x15AD
#define PCI_DEVICE_PVSCSI 0x07C0
#define PCI_DEVICE_VMXNET3 0x07B0

#define PCI_COMMAND_OFFSET 0x04
#define PCI_COMMAND_IO     (1u << 0)
#define PCI_COMMAND_MEMORY (1u << 1)
#define PCI_COMMAND_MASTER (1u << 2)

struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint64_t bar[6];
    uint8_t bar_is_io[6];
};

uint32_t pci_read_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
uint16_t pci_read_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);

int pci_find_device(uint16_t vendor_id, uint16_t device_id, struct pci_device *out_dev);
void pci_enable_bus_master(const struct pci_device *dev);

#endif
