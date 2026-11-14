#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d /tmp/syle-lgig-test.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
apic=${GUEST_APIC_API_DIR:-krnl};devices=${DEVICE_API_DIR:-krnl}
sources="krnl/linux_guest_irq_glue.c krnl/linux_guest_irq_adapter.c krnl/linux_external_irq.c krnl/linux_apic_msr.c $apic/guest_apic.c $apic/lapic.c $apic/lapic_timer.c $devices/ioapic.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -I"$apic" -I"$devices" -Ikrnl -fsanitize=address,undefined -g krnl/host-tests/firmware_timer_irq_composition.c $sources -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -I"$apic" -I"$devices" -Ikrnl -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/linux_guest_irq_glue.c -o "$tmp/model.o"
