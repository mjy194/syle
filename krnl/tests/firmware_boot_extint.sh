#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined "$here/firmware_boot_extint.c" "$here/../firmware_apic_guard.c" "$here/../guest_apic.c" "$here/../lapic.c" "$here/../lapic_timer.c" -I"$here/.." -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
