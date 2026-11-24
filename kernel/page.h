#ifndef SIGIL_OS_PAGE_H
#define SIGIL_OS_PAGE_H

#include <stdint.h>
#include <stddef.h>
#include "contract.h"

#define PAGE_SIZE 4096ULL
#define PAGE_MASK (~(PAGE_SIZE - 1))

/* UEFI Memory Descriptor Types */
#define EFI_RESERVED_MEMORY_TYPE        0
#define EFI_LOADER_CODE                 1
#define EFI_LOADER_DATA                 2
#define EFI_BOOT_SERVICES_CODE          3
#define EFI_BOOT_SERVICES_DATA          4
#define EFI_RUNTIME_SERVICES_CODE       5
#define EFI_RUNTIME_SERVICES_DATA       6
#define EFI_CONVENTIONAL_MEMORY         7
#define EFI_UNUSABLE_MEMORY             8
#define EFI_ACPI_RECLAIM_MEMORY         9
#define EFI_ACPI_MEMORY_NVS             10
#define EFI_MEMORY_MAPPED_IO            11
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE 12
#define EFI_PAL_CODE                    13
#define EFI_PERSISTENT_MEMORY           14

struct efi_memory_descriptor {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

void page_init(const struct boot_info *info);
void *page_alloc(void);
void page_free(void *ptr);
void *page_alloc_contiguous(uint64_t count);
void page_free_contiguous(void *ptr, uint64_t count);
uint64_t page_total_managed(void);
uint64_t page_free_count(void);
uint64_t page_allocated_count(void);
int page_is_reserved_address(uint64_t phys_addr);

void test_page_reserved_excluded(void);
void test_page_oom(void);

#endif
