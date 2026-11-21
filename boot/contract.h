#ifndef SIGIL_OS_BOOT_CONTRACT_H
#define SIGIL_OS_BOOT_CONTRACT_H

#include <stdint.h>

#define BOOT_INFO_VERSION 1u
#define KERNEL_BASE 0x02000000u
#define KERNEL_WINDOW 0x00200000u

struct boot_info {
    uint32_t version;
    uint32_t size;
    uint64_t memory_map;
    uint64_t memory_map_size;
    uint64_t descriptor_size;
    uint32_t descriptor_version;
    uint32_t reserved;
};

_Static_assert(sizeof(struct boot_info) == 40, "boot ABI");

#endif
