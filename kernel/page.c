#include "page.h"
#include "serial.h"

#define MAX_MANAGED_PAGES 8192
#define MAX_RESERVED_RANGES 64

struct mem_range {
    uint64_t start;
    uint64_t end;
};

static uint64_t free_page_stack[MAX_MANAGED_PAGES];
static uint64_t free_stack_top = 0;
static uint64_t total_managed_pages = 0;
static uint64_t allocated_pages = 0;

static struct mem_range reserved_ranges[MAX_RESERVED_RANGES];
static uint32_t reserved_range_count = 0;

static void add_reserved_range(uint64_t start, uint64_t end)
{
    if (reserved_range_count < MAX_RESERVED_RANGES && start < end) {
        reserved_ranges[reserved_range_count].start = start;
        reserved_ranges[reserved_range_count].end = end;
        reserved_range_count++;
    }
}

int page_is_reserved_address(uint64_t phys_addr)
{
    for (uint32_t i = 0; i < reserved_range_count; ++i) {
        if (phys_addr >= reserved_ranges[i].start && phys_addr < reserved_ranges[i].end)
            return 1;
    }
    return 0;
}

static int range_overlaps_reserved(uint64_t start, uint64_t end)
{
    for (uint32_t i = 0; i < reserved_range_count; ++i) {
        if (start < reserved_ranges[i].end && end > reserved_ranges[i].start)
            return 1;
    }
    return 0;
}

void page_init(const struct boot_info *info)
{
    free_stack_top = 0;
    total_managed_pages = 0;
    allocated_pages = 0;
    reserved_range_count = 0;

    /* 1. Reserve low memory: [0x0, 0x100000) (first 1 MiB for IVT, BDA, BIOS) */
    add_reserved_range(0x0, 0x100000);

    /* 2. Reserve kernel memory window: [KERNEL_BASE, KERNEL_BASE + KERNEL_WINDOW) */
    add_reserved_range(KERNEL_BASE, KERNEL_BASE + KERNEL_WINDOW);

    /* 3. Reserve boot_info structure */
    uint64_t info_start = (uint64_t)(uintptr_t)info;
    add_reserved_range(info_start, info_start + sizeof(*info));

    /* 4. Reserve UEFI memory map buffer */
    add_reserved_range(info->memory_map, info->memory_map + info->memory_map_size);

    /* 5. Scan memory map for non-conventional memory and record as reserved */
    uint64_t map_addr = info->memory_map;
    uint64_t desc_size = info->descriptor_size;
    uint64_t num_descriptors = info->memory_map_size / desc_size;

    for (uint64_t i = 0; i < num_descriptors; ++i) {
        const struct efi_memory_descriptor *desc =
            (const struct efi_memory_descriptor *)(uintptr_t)(map_addr + i * desc_size);
        uint64_t start = desc->physical_start;
        uint64_t end = start + desc->number_of_pages * PAGE_SIZE;

        if (desc->type != EFI_CONVENTIONAL_MEMORY) {
            add_reserved_range(start, end);
        }
    }

    /* 6. Collect available 4KiB pages from EFI_CONVENTIONAL_MEMORY that do not overlap reserved regions */
    for (uint64_t i = 0; i < num_descriptors; ++i) {
        const struct efi_memory_descriptor *desc =
            (const struct efi_memory_descriptor *)(uintptr_t)(map_addr + i * desc_size);

        if (desc->type == EFI_CONVENTIONAL_MEMORY) {
            uint64_t page_addr = desc->physical_start;
            for (uint64_t p = 0; p < desc->number_of_pages; ++p, page_addr += PAGE_SIZE) {
                if (free_stack_top >= MAX_MANAGED_PAGES)
                    break;
                if (!range_overlaps_reserved(page_addr, page_addr + PAGE_SIZE)) {
                    free_page_stack[free_stack_top++] = page_addr;
                    total_managed_pages++;
                }
            }
        }
        if (free_stack_top >= MAX_MANAGED_PAGES)
            break;
    }

    if (total_managed_pages == 0)
        boot_stop("STOP no conventional memory pages\r\n");
}

void *page_alloc(void)
{
    if (free_stack_top == 0)
        return 0;
    uint64_t addr = free_page_stack[--free_stack_top];
    allocated_pages++;
    /* Zero the page before returning */
    uint64_t *p = (uint64_t *)(uintptr_t)addr;
    for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); ++i)
        p[i] = 0;
    return (void *)(uintptr_t)addr;
}

void page_free(void *ptr)
{
    if (!ptr)
        return;
    uint64_t addr = (uint64_t)(uintptr_t)ptr;
    if (free_stack_top < MAX_MANAGED_PAGES) {
        free_page_stack[free_stack_top++] = addr;
        if (allocated_pages > 0)
            allocated_pages--;
    }
}

uint64_t page_total_managed(void)
{
    return total_managed_pages;
}

uint64_t page_free_count(void)
{
    return free_stack_top;
}

uint64_t page_allocated_count(void)
{
    return allocated_pages;
}

void test_page_reserved_excluded(void)
{
    /* 1. Test page_is_reserved_address for known reserved addresses */
    if (!page_is_reserved_address(0x0))
        boot_stop("STOP page test: address 0x0 not reserved\r\n");
    if (!page_is_reserved_address(0x80000))
        boot_stop("STOP page test: low memory 0x80000 not reserved\r\n");
    if (!page_is_reserved_address(KERNEL_BASE))
        boot_stop("STOP page test: KERNEL_BASE not reserved\r\n");
    if (!page_is_reserved_address(KERNEL_BASE + 0x1000))
        boot_stop("STOP page test: kernel window not reserved\r\n");

    /* 2. Allocate multiple pages and verify NONE fall in reserved areas */
    void *pages[32];
    for (int i = 0; i < 32; ++i) {
        pages[i] = page_alloc();
        if (!pages[i])
            boot_stop("STOP page test: allocation failed\r\n");
        uint64_t addr = (uint64_t)(uintptr_t)pages[i];
        if (addr < 0x100000)
            boot_stop("STOP page test: allocated low memory page\r\n");
        if (addr >= KERNEL_BASE && addr < KERNEL_BASE + KERNEL_WINDOW)
            boot_stop("STOP page test: allocated kernel window page\r\n");
        if (page_is_reserved_address(addr))
            boot_stop("STOP page test: allocated reserved page\r\n");
    }

    /* Free them back */
    for (int i = 0; i < 32; ++i) {
        page_free(pages[i]);
    }

    serial_write("page test reserved excluded=OK\r\n");
}

void test_page_oom(void)
{
    uint64_t saved_top = free_stack_top;
    /* Temporarily exhaust the free page stack */
    free_stack_top = 0;

    void *oom_page = page_alloc();
    if (oom_page != 0)
        boot_stop("STOP page test: expected NULL on OOM\r\n");

    /* Restore free page stack */
    free_stack_top = saved_top;

    /* Verify allocation succeeds again */
    void *recovered_page = page_alloc();
    if (!recovered_page)
        boot_stop("STOP page test: recovery allocation failed\r\n");
    page_free(recovered_page);

    serial_write("page test oom handled=OK\r\n");
}
