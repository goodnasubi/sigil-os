#!/bin/sh
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
xorriso -as mkisofs -quiet -e esp.img -no-emul-boot -o "$tmp/boot.iso" "$tmp"
mv "$tmp/boot.iso" "$output"
