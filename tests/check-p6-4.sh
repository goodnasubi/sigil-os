#!/bin/sh
# P6-4: 組み込み実機の構成（docs/p6-4-board.md）を QEMU/OVMF で近似して起動試験する。
# USB 接続のストレージから起動し、PVSCSI・VMXNET3 などの VMware 機器は付けない。
# 実機での起動は別に手順（docs/p6-4-verification.md）で行い、この試験は代わりにならない。
set -eu
build=$1
: "${OVMF_CODE:=/usr/share/OVMF/OVMF_CODE_4M.fd}"
: "${OVMF_VARS:=/usr/share/OVMF/OVMF_VARS_4M.fd}"
: "${QEMU_DATA:=/usr/share/qemu}"

img="$build/sigil-os-usb.img"
vars="$build/p6-4.vars.fd"
serial="$build/p6-4.serial"

if [ ! -f "$img" ]; then
    echo "USB image not found: $img" >&2
    exit 1
fi

cp "$OVMF_VARS" "$vars"
: > "$serial"

status=0
timeout 20s qemu-system-x86_64 -L "$QEMU_DATA" -machine q35,accel=tcg -m 256 \
    -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE" \
    -drive "if=pflash,format=raw,file=$vars" \
    -device qemu-xhci \
    -drive "file=$img,if=none,id=usb0,format=raw" \
    -device usb-storage,drive=usb0,bootindex=0 \
    -display none -vga none \
    -serial "file:$serial" -monitor none -nic none -no-reboot || status=$?

observed=$(tr -d '\r' < "$serial" |
    grep -E '^(ExitBootServices |kernel entry|boot-info |page |interrupt |allocator |p6-2 |p6-4 |STOP )' || true)

printf 'p6-4 board run (status %s):\n%s\n' "$status" "$observed"

# ページ数は搭載メモリで変わるため、値は正の整数であることだけを確かめ、比較では N に置き換える。
for key in managed_pages free_pages; do
    if ! printf '%s\n' "$observed" | grep -Eq "^p6-4 memory init: OK .*$key=[1-9][0-9]*"; then
        echo "p6-4 memory report lacks positive $key" >&2
        exit 1
    fi
done
normalized=$(printf '%s\n' "$observed" |
    sed -e 's/managed_pages=[0-9][0-9]*/managed_pages=N/' -e 's/free_pages=[0-9][0-9]*/free_pages=N/')

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
p6-4 memory init: OK managed_pages=N free_pages=N
p6-4 interrupt: OK vector=0x20 count=1
p6-4 uart loopback: OK com1=0x3f8 bytes=8 fifo=on
p6-4 uart rejected: OK
p6-4 board observation: ALL CHECKS PASSED
p6-2 kernel done'

if [ "$status" -ne 124 ]; then
    echo "Expected timeout status 124 (halted kernel), got $status" >&2
    exit 1
fi

if [ "$normalized" != "$expected" ]; then
    echo "Observed output did not match expected sequence!" >&2
    exit 1
fi

echo "p6-4 board (USB boot) QEMU boot and observation test OK"
