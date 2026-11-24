#!/bin/sh
set -eu
build=$1
: "${OVMF_CODE:=/usr/share/OVMF/OVMF_CODE_4M.fd}"
: "${OVMF_VARS:=/usr/share/OVMF/OVMF_VARS_4M.fd}"
: "${QEMU_DATA:=/usr/share/qemu}"

run() {
    name=$1
    expected=$2
    cp "$OVMF_VARS" "$build/$name.vars.fd"
    : > "$build/$name.serial"
    status=0
    timeout 12s qemu-system-x86_64 -L "$QEMU_DATA" -machine q35,accel=tcg -m 256 \
        -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE" \
        -drive "if=pflash,format=raw,file=$build/$name.vars.fd" \
        -cdrom "$build/$name.iso" -boot d -display none -vga none \
        -serial "file:$build/$name.serial" -monitor none -nic none -no-reboot || status=$?
    observed=$(tr -d '\r' < "$build/$name.serial" |
        grep -E '^(ExitBootServices |kernel entry|boot-info |STOP )' || true)
    printf '%s (timeout status %s)\n%s\n' "$name" "$status" "$observed"
    test "$status" -eq 124 && test "$observed" = "$expected"
}

run probe 'ExitBootServices OK
kernel entry
boot-info version=1
boot-info check=OK' || exit 1
run bad-elf 'STOP unsupported ELF' || exit 1
run bad-info 'ExitBootServices OK
kernel entry
STOP boot-info version'
