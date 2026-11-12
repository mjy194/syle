#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Ikrnl -Wall -Wextra -Werror -fsanitize=address,undefined -g krnl/host-tests/pic0_ports.c krnl/pic0_ports.c krnl/pic0.c krnl/linux_pic_mask.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Ikrnl -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/pic0_ports.c -o "$tmp/model.o"
