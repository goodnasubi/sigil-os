#include "contract.h"
#include "serial.h"

static volatile uint64_t initialized = 0x12345678;
static volatile uint64_t cleared;

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
    if (initialized != 0x12345678 || cleared != 0)
        boot_stop("STOP kernel segments\r\n");
    boot_stop("boot-info check=OK\r\n");
}
