#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d /tmp/syle-lig-test.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -Ikrnl -fsanitize=address,undefined -g krnl/host-tests/linux_irq_glue.c krnl/linux_irq_glue.c krnl/linux_external_irq.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -Ikrnl -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/linux_irq_glue.c -o "$tmp/glue.o"
devices=${DEVICE_API_DIR:-krnl}
${CC:-cc} -std=c11 -Wall -Wextra -Werror -I"$devices" -Ikrnl -fsanitize=address,undefined -g krnl/host-tests/linux_irq_glue_devices.c krnl/linux_irq_glue.c krnl/linux_external_irq.c krnl/linux_irq_adapter.c "$devices/lapic.c" "$devices/ioapic.c" -o "$tmp/devices"
ASAN_OPTIONS=detect_leaks=0 "$tmp/devices"
