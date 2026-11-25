EFI_INCLUDE ?= /usr/include/efi
EFI_LIB ?= /usr/lib
BUILD := build
FREESTANDING := -m64 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
	-fno-stack-protector -fno-stack-clash-protection -fno-asynchronous-unwind-tables \
	-mno-red-zone -mgeneral-regs-only
EFI_FLAGS := -isystem $(EFI_INCLUDE) -isystem $(EFI_INCLUDE)/x86_64 \
	-DGNU_EFI_USE_MS_ABI -fpic -fshort-wchar
KERNEL_CFLAGS := $(FREESTANDING) -fno-pie -Iboot -Ikernel

KERNEL_OBJS := $(BUILD)/kernel_main.o \
	$(BUILD)/kernel_page.o \
	$(BUILD)/kernel_interrupt.o \
	$(BUILD)/kernel_interrupt_entry.o \
	$(BUILD)/kernel_alloc.o \
	$(BUILD)/kernel_core.o \
	$(BUILD)/kernel_pci.o \
	$(BUILD)/kernel_pvscsi.o \
	$(BUILD)/kernel_vmxnet3.o \
	$(BUILD)/kernel_device_test.o \
	$(BUILD)/kernel_uart.o \
	$(BUILD)/kernel_board_test.o

.PHONY: all images iso check check-probe check-boot check-kernel check-p6-2 check-p6-3 check-p6-4 check-sigil usb clean
all: $(BUILD)/loader.efi $(BUILD)/kernel.elf $(BUILD)/sigil-os.iso

$(BUILD):
	mkdir -p $@

$(BUILD)/loader.o $(BUILD)/bad-info.o: boot/loader.c boot/contract.h boot/serial.h | $(BUILD)
	$(CC) $(FREESTANDING) $(EFI_FLAGS) $(if $(filter bad-info.o,$(@F)),-DTEST_BAD_BOOT_INFO) -c $< -o $@

$(BUILD)/handoff.o: boot/handoff.S | $(BUILD)
	$(CC) -m64 -c $< -o $@

$(BUILD)/%.so: $(BUILD)/%.o $(BUILD)/handoff.o
	$(LD) -T $(EFI_LIB)/elf_x86_64_efi.lds -shared -Bsymbolic --no-undefined \
		$(EFI_LIB)/crt0-efi-x86_64.o $^ $(EFI_LIB)/libefi.a $(EFI_LIB)/libgnuefi.a -o $@

$(BUILD)/%.efi: $(BUILD)/%.so
	$(OBJCOPY) -j .text -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc \
		--target=efi-app-x86_64 $< $@

OBJCOPY ?= objcopy

$(BUILD)/probe.o: tests/probe.c boot/contract.h boot/serial.h | $(BUILD)
	$(CC) $(FREESTANDING) -fno-pie -Iboot -c $< -o $@

$(BUILD)/probe.elf: $(BUILD)/probe.o tests/probe.ld
	$(LD) -m elf_x86_64 -static --no-undefined -T tests/probe.ld $< -o $@

check-probe: $(BUILD)/probe.elf
	tests/check-probe.sh $<

$(BUILD)/kernel_main.o: kernel/main.c boot/contract.h boot/serial.h kernel/page.h kernel/interrupt.h kernel/alloc.h kernel/core.h kernel/device_test.h kernel/board_test.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_page.o: kernel/page.c kernel/page.h boot/contract.h boot/serial.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_interrupt.o: kernel/interrupt.c kernel/interrupt.h boot/serial.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_interrupt_entry.o: kernel/interrupt_entry.S | $(BUILD)
	$(CC) -m64 -c $< -o $@

$(BUILD)/kernel_alloc.o: kernel/alloc.c kernel/alloc.h kernel/page.h boot/serial.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_core.o: kernel/core.c kernel/core.h kernel/alloc.h boot/serial.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_pci.o: kernel/pci.c kernel/pci.h boot/serial.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_pvscsi.o: kernel/pvscsi.c kernel/pvscsi.h kernel/pci.h kernel/page.h boot/serial.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_vmxnet3.o: kernel/vmxnet3.c kernel/vmxnet3.h kernel/pci.h kernel/page.h boot/serial.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_uart.o: kernel/uart.c kernel/uart.h boot/serial.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_board_test.o: kernel/board_test.c kernel/board_test.h kernel/uart.h kernel/page.h kernel/interrupt.h boot/serial.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_device_test.o: kernel/device_test.c kernel/device_test.h kernel/pci.h kernel/pvscsi.h kernel/vmxnet3.h kernel/alloc.h boot/serial.h | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(KERNEL_OBJS) tests/probe.ld
	$(LD) -m elf_x86_64 -static --no-undefined -T tests/probe.ld $(KERNEL_OBJS) -o $@

check-kernel: $(BUILD)/kernel.elf
	tests/check-kernel.sh $<

check-sigil:
	tests/check-sigil.sh

$(BUILD)/sigil-os.iso: $(BUILD)/loader.efi $(BUILD)/kernel.elf check-kernel
	boot/make-iso.sh $(BUILD)/loader.efi $(BUILD)/kernel.elf $@

images: $(BUILD)/loader.efi $(BUILD)/bad-info.efi check-probe
	boot/make-iso.sh $(BUILD)/loader.efi $(BUILD)/probe.elf $(BUILD)/probe.iso
	boot/make-iso.sh $(BUILD)/loader.efi $(BUILD)/loader.efi $(BUILD)/bad-elf.iso
	boot/make-iso.sh $(BUILD)/bad-info.efi $(BUILD)/probe.elf $(BUILD)/bad-info.iso

check-boot: images
	tests/check-boot.sh $(BUILD)

check-p6-2: $(BUILD)/sigil-os.iso check-sigil
	tests/check-p6-2.sh $(BUILD)

check-p6-3: $(BUILD)/sigil-os.iso check-sigil
	tests/check-p6-3.sh $(BUILD)

$(BUILD)/sigil-os-usb.img: $(BUILD)/loader.efi $(BUILD)/kernel.elf check-kernel
	boot/make-usb.sh $(BUILD)/loader.efi $(BUILD)/kernel.elf $@

usb: $(BUILD)/sigil-os-usb.img

check-p6-4: $(BUILD)/sigil-os-usb.img check-sigil
	tests/check-p6-4.sh $(BUILD)

check: check-boot check-kernel check-p6-2 check-p6-3 check-p6-4 check-sigil

iso: $(BUILD)/loader.efi
	test -n "$(KERNEL_ELF)" && test -f "$(KERNEL_ELF)"
	boot/make-iso.sh $< "$(KERNEL_ELF)" $(BUILD)/sigil-os.iso

clean:
	rm -rf $(BUILD)
