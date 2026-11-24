#include "page.h"
#include "serial.h"

#define MAX_MANAGED_PAGES 8192
#define MAX_RESERVED_RANGES 64

struct mem_range {
    uint64_t start;
    uint64_t end;
};

static uint64_t managed_pages[MAX_MANAGED_PAGES];
static uint8_t page_allocated[MAX_MANAGED_PAGES];
static uint64_t total_managed_pages = 0;
static uint64_t allocated_pages = 0;

static struct mem_range reserved_ranges[MAX_RESERVED_RANGES];
static uint32_t reserved_range_count = 0;

static void sort_managed_pages(uint64_t arr[], int64_t low, int64_t high)
{
    while (low < high) {
        uint64_t pivot = arr[high];
        int64_t i = low - 1;
        for (int64_t j = low; j < high; ++j) {
            if (arr[j] <= pivot) {
                i++;
                uint64_t tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
        uint64_t tmp = arr[i + 1];
        arr[i + 1] = arr[high];
        arr[high] = tmp;
        int64_t p = i + 1;

        if (p - low < high - p) {
            sort_managed_pages(arr, low, p - 1);
            low = p + 1;
        } else {
            sort_managed_pages(arr, p + 1, high);
            high = p - 1;
        }
    }
}

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
                if (total_managed_pages >= MAX_MANAGED_PAGES)
                    break;
                if (!range_overlaps_reserved(page_addr, page_addr + PAGE_SIZE)) {
                    managed_pages[total_managed_pages] = page_addr;
                    page_allocated[total_managed_pages] = 0;
                    total_managed_pages++;
                }
            }
        }
        if (total_managed_pages >= MAX_MANAGED_PAGES)
            break;
    }

    if (total_managed_pages == 0)
        boot_stop("STOP no conventional memory pages\r\n");

    sort_managed_pages(managed_pages, 0, (int64_t)total_managed_pages - 1);
}

void *page_alloc_contiguous(uint64_t count)
{
    if (count == 0 || count > total_managed_pages)
        return 0;

    for (uint64_t i = 0; i + count <= total_managed_pages; ++i) {
        if (page_allocated[i])
            continue;

        int ok = 1;
        for (uint64_t j = 0; j < count; ++j) {
            if (page_allocated[i + j]) {
                ok = 0;
                i += j;
                break;
            }
            if (j > 0 && managed_pages[i + j] != managed_pages[i + j - 1] + PAGE_SIZE) {
                ok = 0;
                i += j - 1;
                break;
            }
        }

        if (ok) {
            for (uint64_t j = 0; j < count; ++j) {
                page_allocated[i + j] = 1;
                uint64_t *p = (uint64_t *)(uintptr_t)managed_pages[i + j];
                for (uint32_t k = 0; k < PAGE_SIZE / sizeof(uint64_t); ++k)
                    p[k] = 0;
            }
            allocated_pages += count;
            return (void *)(uintptr_t)managed_pages[i];
        }
    }
    return 0;
}

void page_free_contiguous(void *ptr, uint64_t count)
{
    if (!ptr || count == 0)
        return;

    uint64_t addr = (uint64_t)(uintptr_t)ptr;
    int64_t low = 0;
    int64_t high = (int64_t)total_managed_pages - 1;
    int64_t found = -1;
    while (low <= high) {
        int64_t mid = low + (high - low) / 2;
        if (managed_pages[mid] == addr) {
            found = mid;
            break;
        } else if (managed_pages[mid] < addr) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    if (found < 0)
        boot_stop("STOP invalid page_free address\r\n");

    for (uint64_t j = 0; j < count; ++j) {
        uint64_t idx = (uint64_t)found + j;
        if (idx < total_managed_pages) {
            if (page_allocated[idx]) {
                page_allocated[idx] = 0;
                if (allocated_pages > 0)
                    allocated_pages--;
            }
        }
    }
}

void *page_alloc(void)
{
    return page_alloc_contiguous(1);
}

void page_free(void *ptr)
{
    page_free_contiguous(ptr, 1);
}

uint64_t page_total_managed(void)
{
    return total_managed_pages;
}

uint64_t page_free_count(void)
{
    return total_managed_pages > allocated_pages ? total_managed_pages - allocated_pages : 0;
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
    /* Temporarily mark all pages as allocated */
    uint64_t saved_allocated = allocated_pages;
    for (uint64_t i = 0; i < total_managed_pages; ++i) {
        page_allocated[i] = 1;
    }
    allocated_pages = total_managed_pages;

    void *oom_page = page_alloc();
    if (oom_page != 0)
        boot_stop("STOP page test: expected NULL on OOM\r\n");

    /* Restore free state */
    for (uint64_t i = 0; i < total_managed_pages; ++i) {
        page_allocated[i] = 0;
    }
    allocated_pages = 0;

    /* Verify allocation succeeds again */
    void *recovered_page = page_alloc();
    if (!recovered_page)
        boot_stop("STOP page test: recovery allocation failed\r\n");
    page_free(recovered_page);

    allocated_pages = saved_allocated;

    serial_write("page test oom handled=OK\r\n");
}
