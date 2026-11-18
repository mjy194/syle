#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined "$here/firmware_boot_debug.c" "$here/../firmware_boot_debug.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
cc -std=c11 -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -c "$here/../firmware_boot_debug.c" -o "$tmp/debug.o"
