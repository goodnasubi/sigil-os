#!/bin/sh
# USB メモリに書き込める起動イメージ（GPT 付き）を作る。EFI システムパーティションに
# \EFI\BOOT\BOOTX64.EFI と \kernel.elf を置く（起動契約は docs/boot-contract.md）。
# 実機へは `dd if=build/sigil-os-usb.img of=/dev/sdX bs=4M conv=fsync` で書く。
set -eu
loader=$1
kernel=$2
output=$3
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
truncate -s 16M "$tmp/esp.img"
mformat -i "$tmp/esp.img" ::
mmd -i "$tmp/esp.img" ::/EFI ::/EFI/BOOT
mcopy -i "$tmp/esp.img" "$loader" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$tmp/esp.img" "$kernel" ::/kernel.elf
mkdir "$tmp/empty"
xorriso -as mkisofs -quiet -V SIGILOS -o "$tmp/usb.img" \
    -append_partition 2 0xef "$tmp/esp.img" -appended_part_as_gpt -iso_mbr_part_type 0x00 \
    -e --interval:appended_partition_2:all:: -no-emul-boot "$tmp/empty"
mv "$tmp/usb.img" "$output"
