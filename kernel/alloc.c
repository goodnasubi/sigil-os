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
    uint64_t align;
    uint64_t class_index; /* 0..7 for small classes, 8 for large */
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
        hdr->align = align;
        hdr->class_index = (uint64_t)cls;

        live_blocks_count++;
        live_bytes_count += size;

        return (void *)(hdr + 1);
    } else {
        /* Large block allocation: allocate pages */
        uint64_t actual_align = align < 32 ? 32 : align;
        uint64_t total_needed = sizeof(struct block_header) + size + actual_align;
        uint64_t pages_needed = (total_needed + PAGE_SIZE - 1) / PAGE_SIZE;

        void *first_page = page_alloc();
        if (!first_page)
            return 0; /* OOM */

        /* Allocate additional consecutive pages if needed */
        for (uint64_t p = 1; p < pages_needed; ++p) {
            void *next_p = page_alloc();
            if (!next_p) {
                /* OOM: free previously allocated pages */
                page_free(first_page);
                return 0;
            }
        }

        struct block_header *hdr = (struct block_header *)first_page;
        hdr->magic = SIGIL_ALLOC_MAGIC;
        hdr->size = size;
        hdr->align = align;
        hdr->class_index = 8; /* Large */

        live_blocks_count++;
        live_bytes_count += size;

        return (void *)(hdr + 1);
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
    uint64_t cls = hdr->class_index;
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
        /* Large block: return page */
        page_free((void *)hdr);
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
    /* Requesting size exceeding MAX_ALLOC_SIZE or wrapping */
    void *oom1 = sigil_alloc(0xFFFFFFFFFFFFFFFFULL, 16);
    if (oom1 != 0)
        boot_stop("STOP allocator test: expected NULL on max size request\r\n");

    void *oom2 = sigil_alloc(0x7FFFFFFFFFFFFFFFULL, 16);
    if (oom2 != 0)
        boot_stop("STOP allocator test: expected NULL on huge size request\r\n");

    serial_write("allocator oom handled=OK\r\n");
}
