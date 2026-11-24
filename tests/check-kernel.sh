#!/bin/sh
set -eu
export LC_ALL=C
file=$1
headers=$(readelf -hlWd "$file")
symbols=$(nm "$file")
disassembly=$(objdump -d --no-show-raw-insn "$file")

printf '%s\n' "$headers" "$symbols"

# 1. Must be static 64-bit ELF executable
if ! printf '%s\n' "$headers" | grep -Eq 'Type: +EXEC'; then
    echo "kernel ELF check failed: not EXEC" >&2
    exit 1
fi

# 2. No dynamic sections or interpreters
if printf '%s\n' "$headers" | grep -Eq 'INTERP|DYNAMIC|NEEDED'; then
    echo "kernel ELF check failed: contains dynamic linking sections" >&2
    exit 1
fi

# 3. No undefined symbols
if [ -n "$(nm -u "$file")" ]; then
    echo "kernel ELF check failed: undefined symbols found" >&2
    exit 1
fi

# 4. No host Linux system calls or Linux runtime
if printf '%s\n' "$symbols" | grep -Ei 'sigil_start|sigil_run|sigil_switch_stack|__libc'; then
    echo "kernel ELF check failed: host Linux runtime symbols found" >&2
    exit 1
fi

if printf '%s\n' "$disassembly" | grep -Ei '[[:space:]](syscall|sysenter)([[:space:]]|$)'; then
    echo "kernel ELF check failed: host syscall/sysenter instruction found" >&2
    exit 1
fi

echo 'kernel ELF checks OK'
