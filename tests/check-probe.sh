#!/bin/sh
set -eu
export LC_ALL=C
file=$1
headers=$(readelf -hlWd "$file")
symbols=$(nm "$file")
disassembly=$(objdump -d --no-show-raw-insn "$file")
printf '%s\n' "$headers" "$symbols"
if ! printf '%s\n' "$headers" | grep -Eq 'Type: +EXEC' ||
    printf '%s\n' "$headers" | grep -Eq 'INTERP|DYNAMIC|NEEDED' ||
    [ -n "$(nm -u "$file")" ] ||
    printf '%s\n' "$symbols" | grep -Ei 'alloc|(^|[_.])free|syscall' ||
    printf '%s\n' "$disassembly" | grep -Ei '[[:space:]](syscall|sysenter|int)([[:space:]]|$)'
then
    echo 'probe ELF checks failed' >&2
    exit 1
fi
echo 'probe ELF checks OK'
