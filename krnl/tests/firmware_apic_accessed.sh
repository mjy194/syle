#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
 "$here/../firmware_layout.c" "$here/../firmware_memory.c" "$here/../firmware_mmio_memory.c" \
 "$here/../guest_mmio_mov.c" "$here/../firmware_apic_guard.c" "$here/../guest_apic.c" "$here/../lapic.c" "$here/../lapic_timer.c" \
 "$here/../firmware_timer_100m.c" "$here/../firmware_timer_100m_mmio.c" "$here/../firmware_accessed_cas.c" \
 "$here/../firmware_apic_accessed.c" "$here/../firmware_apic_accessed_transaction.c" "$here/firmware_apic_accessed.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
for file in firmware_apic_accessed firmware_apic_accessed_transaction; do
 ${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin -c "$here/../$file.c" -o "$tmp/$file.o"
done
