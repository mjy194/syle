#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined "$here/linux_boot.c" "$here/../linux_boot.c" -o "$out/test"
ASAN_OPTIONS=detect_leaks=0 "$out/test"
cc -std=c11 -O2 -ffreestanding -mno-red-zone -mgeneral-regs-only -Wall -Wextra -Werror -c "$here/../linux_boot.c" -o "$out/boot.o"
