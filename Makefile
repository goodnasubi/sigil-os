EFI_INCLUDE ?= /usr/include/efi
EFI_LIB ?= /usr/lib
BUILD := build
FREESTANDING := -m64 -O2 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
	-fno-stack-protector -fno-stack-clash-protection -fno-asynchronous-unwind-tables \
	-mno-red-zone -mgeneral-regs-only
EFI_FLAGS := -isystem $(EFI_INCLUDE) -isystem $(EFI_INCLUDE)/x86_64 \
	-DGNU_EFI_USE_MS_ABI -fpic -fshort-wchar

.PHONY: all images iso check-probe check-boot clean
all: $(BUILD)/loader.efi

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

images: $(BUILD)/loader.efi $(BUILD)/bad-info.efi check-probe
	boot/make-iso.sh $(BUILD)/loader.efi $(BUILD)/probe.elf $(BUILD)/probe.iso
	boot/make-iso.sh $(BUILD)/loader.efi $(BUILD)/loader.efi $(BUILD)/bad-elf.iso
	boot/make-iso.sh $(BUILD)/bad-info.efi $(BUILD)/probe.elf $(BUILD)/bad-info.iso

check-boot: images
	tests/check-boot.sh $(BUILD)

iso: $(BUILD)/loader.efi
	test -n "$(KERNEL_ELF)" && test -f "$(KERNEL_ELF)"
	boot/make-iso.sh $< "$(KERNEL_ELF)" $(BUILD)/sigil-os.iso

clean:
	rm -rf $(BUILD)
