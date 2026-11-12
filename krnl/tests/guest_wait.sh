#!/bin/sh
# Host-only component; not wired into runtime.
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Ikrnl -Wall -Wextra -Werror -fsanitize=address,undefined -g krnl/host-tests/guest_wait.c krnl/guest_wait.c  -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Ikrnl -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/guest_wait.c -o "$tmp/model.o"
