#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT HUP INT TERM
api=${SESSION_API_DIR:-krnl}
${CC:-cc} -std=c11 -Wall -Wextra -Werror -Ikrnl -I"$api" -fsanitize=address,undefined -g krnl/host-tests/linux_nonpresent_pf.c krnl/linux_nonpresent_pf.c krnl/linux_mmio_memory.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -Ikrnl -I"$api" -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/linux_nonpresent_pf.c -o "$tmp/native.o"
