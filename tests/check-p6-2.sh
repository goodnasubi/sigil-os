#!/bin/sh
set -eu
build=$1
: "${OVMF_CODE:=/usr/share/OVMF/OVMF_CODE_4M.fd}"
: "${OVMF_VARS:=/usr/share/OVMF/OVMF_VARS_4M.fd}"
: "${QEMU_DATA:=/usr/share/qemu}"

iso="$build/sigil-os.iso"
vars="$build/p6-2.vars.fd"
serial="$build/p6-2.serial"

if [ ! -f "$iso" ]; then
    echo "ISO file not found: $iso" >&2
    exit 1
fi

cp "$OVMF_VARS" "$vars"
: > "$serial"

status=0
timeout 15s qemu-system-x86_64 -L "$QEMU_DATA" -machine q35,accel=tcg -m 256 \
    -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE" \
    -drive "if=pflash,format=raw,file=$vars" \
    -cdrom "$iso" -boot d -display none -vga none \
    -serial "file:$serial" -monitor none -nic none -no-reboot || status=$?

observed=$(tr -d '\r' < "$serial" |
    grep -E '^(ExitBootServices |kernel entry|boot-info |page |interrupt |allocator |p6-2 |STOP )' || true)

printf 'p6-2 kernel run (status %s):\n%s\n' "$status" "$observed"

expected='ExitBootServices OK
kernel entry
boot-info version=1
boot-info check=OK
page manager init: OK
page test reserved excluded=OK
page test oom handled=OK
interrupt init: OK
interrupt test received=OK vector=0x20 count=1
allocator init: OK
allocator core list test=OK
allocator core string test=OK
allocator oom handled=OK
p6-2 minimal kernel: ALL CHECKS PASSED
p6-2 kernel done'

if [ "$status" -ne 124 ]; then
    echo "Expected timeout status 124 (halted kernel), got $status" >&2
    exit 1
fi

if [ "$observed" != "$expected" ]; then
    echo "Observed output did not match expected sequence!" >&2
    exit 1
fi

echo "p6-2 minimal kernel VM boot and execution test OK"
