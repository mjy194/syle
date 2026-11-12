#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d /tmp/syle-lmm-test.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g krnl/host-tests/linux_mmio_memory.c krnl/linux_mmio_memory.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/linux_mmio_memory.c -o "$tmp/model.o"
