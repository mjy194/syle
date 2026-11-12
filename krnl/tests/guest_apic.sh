#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
cc=${CC:-cc}
"$cc" -std=c11 -Wall -Wextra -Werror -fsanitize=undefined \
  "$root/krnl/guest_apic.c" "$root/krnl/lapic.c" "$root/krnl/lapic_timer.c" \
  "$root/krnl/tests/guest_apic.c" -o "$build/test"
"$build/test"
"$cc" -std=c11 -Wall -Wextra -Werror -ffreestanding -mno-red-zone \
  -c "$root/krnl/guest_apic.c" -o "$build/model.o"
