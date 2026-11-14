#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
for flags in "" "-DSYLE_FMM_DE_CONFIG"; do
${CC:-cc} $flags -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
 "$here/../firmware_layout.c" "$here/../firmware_memory.c" "$here/../firmware_mmio_memory.c" "$here/../guest_mmio_mov.c" "$here/../firmware_clock_apic_transaction.c" "$here/../firmware_apic_guard.c" "$here/../firmware_timer_100m.c" "$here/../firmware_timer_100m_mmio.c" "$here/../guest_apic.c" "$here/../lapic.c" "$here/../lapic_timer.c" "$here/../firmware_irq_apic.c" "$here/../firmware_timer_irq_session.c" "$here/../firmware_timer_irq_owner.c" "$here/../linux_external_irq.c" "$here/../firmware_irq_cr8_write.c" "$here/firmware_irq_cr8_write.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
done
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
 -c "$here/../firmware_irq_cr8_write.c" -o "$tmp/memory.o"
