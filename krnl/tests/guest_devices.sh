#!/bin/sh
# Host-only device-model checks. Does not launch a guest or invoke QEMU.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
cc=${CC:-cc}
for test in lapic_timer guest_timer; do
    "$cc" -std=c11 -Wall -Wextra -Werror -fsanitize=undefined \
        "$root/krnl/lapic_timer.c" "$root/krnl/tests/$test.c" -o "$build/$test"
    "$build/$test"
done
for test in ioapic guest_ioapic; do
    "$cc" -std=c11 -Wall -Wextra -Werror -fsanitize=undefined \
        "$root/krnl/ioapic.c" "$root/krnl/device_bus.c" \
        "$root/krnl/tests/$test.c" -o "$build/$test"
    "$build/$test"
done
"$cc" -std=c11 -Wall -Wextra -Werror -fsanitize=undefined \
    "$root/krnl/lapic.c" "$root/krnl/ioapic.c" \
    "$root/krnl/tests/lapic_external.c" -o "$build/lapic_external"
"$build/lapic_external"
for unit in lapic_timer ioapic lapic; do
    "$cc" -std=c11 -Wall -Wextra -Werror -ffreestanding -mno-red-zone \
        -c "$root/krnl/$unit.c" -o "$build/$unit.o"
done
