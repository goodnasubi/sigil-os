#include "alloc.h"
#include "page.h"
#include "serial.h"

#define NUM_SIZE_CLASSES 8
static const uint64_t size_classes[NUM_SIZE_CLASSES] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};

struct block_header {
    uint64_t magic;
    uint64_t size;
    uint32_t align;
    uint32_t page_offset; /* offset in bytes from first_page to this header */
    uint32_t class_index; /* 0..7 for small classes, 8 for large */
    uint32_t page_count;  /* for large blocks: number of contiguous pages */
};

_Static_assert(sizeof(struct block_header) == 32, "block header 32B aligned");

struct freelist_node {
    struct freelist_node *next;
};

static struct freelist_node *freelists[NUM_SIZE_CLASSES];
static uint64_t live_blocks_count = 0;
static uint64_t live_bytes_count = 0;

void alloc_init(void)
{
    for (int i = 0; i < NUM_SIZE_CLASSES; ++i) {
        freelists[i] = 0;
    }
    live_blocks_count = 0;
    live_bytes_count = 0;
}

static int get_size_class(uint64_t size, uint64_t align)
{
    if (align > 16 || size > 2048)
        return -1;
    for (int i = 0; i < NUM_SIZE_CLASSES; ++i) {
        if (size <= size_classes[i])
            return i;
    }
    return -1;
}

void *sigil_alloc(uint64_t size, uint64_t align)
{
    if (size == 0 || size > MAX_ALLOC_SIZE)
        return 0;
    if (align == 0 || (align & (align - 1)) != 0)
        return 0;
    if (align > MAX_ALLOC_SIZE)
        return 0;

    int cls = get_size_class(size, align);
    if (cls >= 0) {
        /* Small block allocation */
        uint64_t slot_size = sizeof(struct block_header) + size_classes[cls];
        if (!freelists[cls]) {
            /* Refill freelist from a fresh 4KiB page */
            void *page = page_alloc();
            if (!page)
                return 0; /* OOM */
            uint64_t count = PAGE_SIZE / slot_size;
            uint8_t *cur = (uint8_t *)page;
            for (uint64_t i = 0; i < count; ++i) {
                struct freelist_node *node = (struct freelist_node *)(uintptr_t)cur;
                node->next = freelists[cls];
                freelists[cls] = node;
                cur += slot_size;
            }
        }
        if (!freelists[cls])
            return 0;

        struct block_header *hdr = (struct block_header *)(uintptr_t)freelists[cls];
        freelists[cls] = freelists[cls]->next;

        hdr->magic = SIGIL_ALLOC_MAGIC;
        hdr->size = size;
        hdr->align = (uint32_t)align;
        hdr->page_offset = 0;
        hdr->class_index = (uint32_t)cls;
        hdr->page_count = 0;

        live_blocks_count++;
        live_bytes_count += size;

        return (void *)(hdr + 1);
    } else {
        /* Large block allocation: allocate contiguous pages */
        uint64_t actual_align = align < 32 ? 32 : align;
        uint64_t total_needed = sizeof(struct block_header) + actual_align + size;
        uint64_t pages_needed = (total_needed + PAGE_SIZE - 1) / PAGE_SIZE;

        void *first_page = page_alloc_contiguous(pages_needed);
        if (!first_page)
            return 0; /* OOM */

        uintptr_t base = (uintptr_t)first_page;
        uintptr_t min_payload = base + sizeof(struct block_header);
        uintptr_t payload = (min_payload + (actual_align - 1)) & ~(actual_align - 1);

        struct block_header *hdr = ((struct block_header *)payload) - 1;
        hdr->magic = SIGIL_ALLOC_MAGIC;
        hdr->size = size;
        hdr->align = (uint32_t)align;
        hdr->page_offset = (uint32_t)((uintptr_t)hdr - base);
        hdr->class_index = 8; /* Large */
        hdr->page_count = (uint32_t)pages_needed;

        live_blocks_count++;
        live_bytes_count += size;

        return (void *)payload;
    }
}

void sigil_free(void *ptr, uint64_t size, uint64_t align)
{
    (void)size;
    (void)align;
    if (!ptr)
        return;

    struct block_header *hdr = ((struct block_header *)ptr) - 1;
    if (hdr->magic != SIGIL_ALLOC_MAGIC)
        boot_stop("STOP invalid free magic\r\n");

    uint64_t original_size = hdr->size;
    uint32_t cls = hdr->class_index;
    hdr->magic = 0; /* invalidate */

    if (live_blocks_count > 0)
        live_blocks_count--;
    if (live_bytes_count >= original_size)
        live_bytes_count -= original_size;
    else
        live_bytes_count = 0;

    if (cls < NUM_SIZE_CLASSES) {
        struct freelist_node *node = (struct freelist_node *)(uintptr_t)hdr;
        node->next = freelists[cls];
        freelists[cls] = node;
    } else {
        /* Large block: return all allocated contiguous pages */
        void *first_page = (void *)((uintptr_t)hdr - hdr->page_offset);
        page_free_contiguous(first_page, hdr->page_count);
    }
}

uint64_t alloc_live_blocks(void)
{
    return live_blocks_count;
}

uint64_t alloc_live_bytes(void)
{
    return live_bytes_count;
}

void test_allocator_oom(void)
{
    uint64_t initial_blocks = alloc_live_blocks();
    uint64_t initial_pages = page_allocated_count();

    /* Test multi-page contiguous allocation and tracking */
    void *multi = sigil_alloc(5000, 8);
    if (!multi)
        boot_stop("STOP allocator test: multi-page alloc failed\r\n");
    if (page_allocated_count() <= initial_pages + 1)
        boot_stop("STOP allocator test: multi-page should consume > 1 pages\r\n");

    uint8_t *p = (uint8_t *)multi;
    for (uint64_t i = 0; i < 5000; ++i) {
        p[i] = (uint8_t)(i & 0xFF);
    }
    for (uint64_t i = 0; i < 5000; ++i) {
        if (p[i] != (uint8_t)(i & 0xFF))
            boot_stop("STOP allocator test: multi-page memory corruption\r\n");
    }

    sigil_free(multi, 5000, 8);
    if (alloc_live_blocks() != initial_blocks)
        boot_stop("STOP allocator test: multi-page live blocks leak\r\n");
    if (page_allocated_count() != initial_pages)
        boot_stop("STOP allocator test: multi-page page leak\r\n");

    /* Test alignment > 32 for large blocks */
    void *aligned64 = sigil_alloc(1, 64);
    if (!aligned64)
        boot_stop("STOP allocator test: align 64 alloc failed\r\n");
    if (((uintptr_t)aligned64 % 64) != 0)
        boot_stop("STOP allocator test: align 64 misaligned\r\n");
    sigil_free(aligned64, 1, 64);

    void *aligned128 = sigil_alloc(1, 128);
    if (!aligned128)
        boot_stop("STOP allocator test: align 128 alloc failed\r\n");
    if (((uintptr_t)aligned128 % 128) != 0)
        boot_stop("STOP allocator test: align 128 misaligned\r\n");
    sigil_free(aligned128, 1, 128);

    if (alloc_live_blocks() != initial_blocks || page_allocated_count() != initial_pages)
        boot_stop("STOP allocator test: aligned alloc leak\r\n");

    /* Requesting size exceeding MAX_ALLOC_SIZE or wrapping */
    void *oom1 = sigil_alloc(0xFFFFFFFFFFFFFFFFULL, 16);
    if (oom1 != 0)
        boot_stop("STOP allocator test: expected NULL on max size request\r\n");

    void *oom2 = sigil_alloc(0x7FFFFFFFFFFFFFFFULL, 16);
    if (oom2 != 0)
        boot_stop("STOP allocator test: expected NULL on huge size request\r\n");

    serial_write("allocator oom handled=OK\r\n");
}
