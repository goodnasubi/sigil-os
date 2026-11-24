#include <efi.h>
#include <efiprot.h>
#include <elf.h>
#include "contract.h"
#include "serial.h"

#define PAGE_SIZE 4096u
#define MAX_PHDRS 32u

static _Alignas(8) unsigned char image_bytes[KERNEL_WINDOW + 1];
static _Alignas(8) unsigned char memory_map[128 * 1024];
static _Alignas(16) unsigned char kernel_stack[64 * 1024];
static struct boot_info info;

extern __attribute__((noreturn)) void enter_kernel(
    uint64_t entry, const struct boot_info *boot_info, void *stack_top);

static void copy_bytes(void *dest, const void *src, UINTN count)
{
    unsigned char *out = dest;
    const unsigned char *in = src;
    for (UINTN i = 0; i < count; ++i)
        out[i] = in[i];
}

static UINTN read_kernel(EFI_HANDLE image, EFI_BOOT_SERVICES *bs)
{
    EFI_GUID loaded_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *loaded;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_FILE_PROTOCOL *root, *file;
    UINTN size = sizeof(image_bytes);

    if (EFI_ERROR(bs->HandleProtocol(image, &loaded_guid, (void **)&loaded)) ||
        EFI_ERROR(bs->HandleProtocol(loaded->DeviceHandle, &fs_guid, (void **)&fs)) ||
        EFI_ERROR(fs->OpenVolume(fs, &root)))
        boot_stop("STOP ESP\r\n");

    if (EFI_ERROR(root->Open(root, &file, L"\\kernel.elf", EFI_FILE_MODE_READ, 0)))
        boot_stop("STOP kernel file\r\n");
    EFI_STATUS status = file->Read(file, &size, image_bytes);
    file->Close(file);
    root->Close(root);
    if (EFI_ERROR(status))
        boot_stop("STOP kernel read\r\n");
    return size;
}

static void reject_elf(void)
{
    boot_stop("STOP unsupported ELF\r\n");
}

static uint64_t load_kernel(EFI_BOOT_SERVICES *bs, UINTN size)
{
    Elf64_Ehdr eh;
    Elf64_Phdr segments[MAX_PHDRS];
    unsigned int count = 0;
    int entry_in_text = 0;
    uint64_t top = KERNEL_BASE;

    if (size < sizeof(eh) || size >= sizeof(image_bytes))
        reject_elf();
    copy_bytes(&eh, image_bytes, sizeof(eh));
    const unsigned char *id = eh.e_ident;
    if (id[EI_MAG0] != ELFMAG0 || id[EI_MAG1] != ELFMAG1 ||
        id[EI_MAG2] != ELFMAG2 || id[EI_MAG3] != ELFMAG3 ||
        id[EI_CLASS] != ELFCLASS64 || id[EI_DATA] != ELFDATA2LSB ||
        id[EI_VERSION] != EV_CURRENT || id[EI_OSABI] != ELFOSABI_NONE ||
        eh.e_version != EV_CURRENT || eh.e_machine != EM_X86_64 ||
        eh.e_type != ET_EXEC || eh.e_ehsize != sizeof(eh) ||
        eh.e_phentsize != sizeof(Elf64_Phdr) || eh.e_phnum == 0 ||
        eh.e_phnum > MAX_PHDRS || eh.e_phoff < sizeof(eh) || eh.e_phoff > size ||
        eh.e_phnum > (size - eh.e_phoff) / sizeof(Elf64_Phdr))
        reject_elf();

    for (unsigned int i = 0; i < eh.e_phnum; ++i) {
        Elf64_Phdr ph;
        copy_bytes(&ph, image_bytes + eh.e_phoff + i * sizeof(ph), sizeof(ph));
        if (ph.p_type == PT_NULL || ph.p_type == PT_NOTE ||
            ph.p_type == PT_PHDR || ph.p_type == PT_GNU_STACK)
            continue;
        if (ph.p_type != PT_LOAD || ph.p_vaddr != ph.p_paddr ||
            ph.p_vaddr < KERNEL_BASE || ph.p_vaddr >= KERNEL_BASE + KERNEL_WINDOW ||
            ph.p_memsz > KERNEL_BASE + KERNEL_WINDOW - ph.p_vaddr ||
            ph.p_filesz > ph.p_memsz || ph.p_offset > size ||
            ph.p_filesz > size - ph.p_offset ||
            (ph.p_align > 1 &&
             ((ph.p_align & (ph.p_align - 1)) ||
              (ph.p_vaddr % ph.p_align != ph.p_offset % ph.p_align))))
            reject_elf();
        if (!ph.p_memsz)
            continue;
        uint64_t end = ph.p_vaddr + ph.p_memsz;
        for (unsigned int j = 0; j < count; ++j) {
            if (ph.p_vaddr < segments[j].p_vaddr + segments[j].p_memsz &&
                segments[j].p_vaddr < end)
                reject_elf();
        }
        if ((ph.p_flags & PF_X) && eh.e_entry >= ph.p_vaddr &&
            eh.e_entry - ph.p_vaddr < ph.p_filesz)
            entry_in_text = 1;
        segments[count++] = ph;
        if (end > top)
            top = end;
    }
    if (!entry_in_text)
        reject_elf();

    EFI_PHYSICAL_ADDRESS base = KERNEL_BASE;
    if (EFI_ERROR(bs->AllocatePages(AllocateAddress, EfiLoaderCode,
                                  (top - base + PAGE_SIZE - 1) / PAGE_SIZE, &base)))
        boot_stop("STOP kernel pages\r\n");
    for (unsigned int i = 0; i < count; ++i) {
        const Elf64_Phdr *ph = &segments[i];
        unsigned char *dest = (void *)(uintptr_t)ph->p_vaddr;
        copy_bytes(dest, image_bytes + ph->p_offset, ph->p_filesz);
        for (uint64_t j = ph->p_filesz; j < ph->p_memsz; ++j)
            dest[j] = 0;
    }
    return eh.e_entry;
}

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system)
{
    EFI_BOOT_SERVICES *bs = system->BootServices;
    serial_init();
    uint64_t entry = load_kernel(bs, read_kernel(image, bs));
    info.version = BOOT_INFO_VERSION;
#ifdef TEST_BAD_BOOT_INFO
    ++info.version;
#endif
    info.size = sizeof(info);
    info.memory_map = (uintptr_t)memory_map;

    for (unsigned int attempt = 0; attempt < 2; ++attempt) {
        UINTN size = sizeof(memory_map), key, stride;
        UINT32 version;
        if (EFI_ERROR(bs->GetMemoryMap(&size, (EFI_MEMORY_DESCRIPTOR *)memory_map,
                                      &key, &stride, &version)))
            boot_stop("STOP memory map\r\n");
        info.memory_map_size = size;
        info.descriptor_size = stride;
        info.descriptor_version = version;
        EFI_STATUS status = bs->ExitBootServices(image, key);
        if (status == EFI_SUCCESS) {
            __asm__ volatile("cli" : : : "memory");
            serial_write("ExitBootServices OK\r\n");
            enter_kernel(entry, &info, kernel_stack + sizeof(kernel_stack));
        }
        if (status != EFI_INVALID_PARAMETER)
            break;
    }
    boot_stop("STOP ExitBootServices\r\n");
}
