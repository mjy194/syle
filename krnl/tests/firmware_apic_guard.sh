#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
sources="$here/../firmware_apic_guard.c $here/../guest_apic.c $here/../lapic.c $here/../lapic_timer.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined $sources "$here/firmware_apic_guard.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin -c "$here/../firmware_apic_guard.c" -o "$tmp/guard.o"
