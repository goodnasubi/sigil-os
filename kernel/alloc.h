#ifndef SIGIL_OS_ALLOC_H
#define SIGIL_OS_ALLOC_H

#include <stdint.h>
#include <stddef.h>

#define SIGIL_ALLOC_MAGIC 0x5349474C414C0000ULL
#define MAX_ALLOC_SIZE    0x40000000ULL /* 1 GiB limit */

void alloc_init(void);
void *sigil_alloc(uint64_t size, uint64_t align);
void sigil_free(void *ptr, uint64_t size, uint64_t align);
uint64_t alloc_live_blocks(void);
uint64_t alloc_live_bytes(void);

void test_allocator_oom(void);

#endif
