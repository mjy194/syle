#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Ikrnl -Wall -Wextra -Werror -fsanitize=address,undefined -g krnl/host-tests/linux_port80.c krnl/linux_port80.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
cc -std=c11 -Ikrnl -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/linux_port80.c -o "$tmp/model.o"
