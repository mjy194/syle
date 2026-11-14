#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -O1 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
 "$here/linux_ept_probe.c" "$here/../linux_ept_probe.c" "$here/../linux_ept_probe_guest.S" \
 "$here/../linux_guest_memory.c" "$here/../linux_boot.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector \
 -mgeneral-regs-only -mno-red-zone -Wframe-larger-than=1024 -c "$here/../linux_ept_probe.c" -o "$tmp/module.o"
