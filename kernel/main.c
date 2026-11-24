#include "contract.h"
#include "serial.h"
#include "page.h"
#include "interrupt.h"
#include "alloc.h"
#include "core.h"

__attribute__((noreturn)) void kernel_entry(const struct boot_info *info)
{
    serial_write("kernel entry\r\n");
    if (!info || info->version != BOOT_INFO_VERSION)
        boot_stop("STOP boot-info version\r\n");
    serial_write("boot-info version=1\r\n");

    if (info->size != sizeof(*info) || info->reserved ||
        !info->memory_map || info->memory_map % 8 ||
        info->memory_map_size > UINT64_MAX - info->memory_map ||
        info->descriptor_version != 1 || info->descriptor_size < 40 ||
        info->descriptor_size % 8 || info->memory_map_size < info->descriptor_size ||
        info->memory_map_size % info->descriptor_size)
        boot_stop("STOP boot-info fields\r\n");
    serial_write("boot-info check=OK\r\n");

    /* 1. Initialize page management (excluding reserved areas) */
    page_init(info);
    serial_write("page manager init: OK\r\n");

    /* 2. Test: page manager does not allocate reserved areas */
    test_page_reserved_excluded();

    /* 3. Test: page manager resource exhaustion handling */
    test_page_oom();

    /* 4. Initialize interrupts (IDT setup and handlers) */
    interrupt_init();
    serial_write("interrupt init: OK\r\n");

    /* 5. Test: interrupt receipt */
    test_interrupt_receive();

    /* 6. Initialize heap allocator */
    alloc_init();
    serial_write("allocator init: OK\r\n");

    /* 7. Use Sigil allocation-dependent core (List and String) */
    test_sigil_core_list();
    test_sigil_core_string();

    /* 8. Test: allocator resource exhaustion handling */
    test_allocator_oom();

    serial_write("p6-2 minimal kernel: ALL CHECKS PASSED\r\n");
    boot_stop("p6-2 kernel done\r\n");
}
