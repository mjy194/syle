#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
out="$tmp/test"
mkdir -p "$(dirname "$out")"
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined "$here/linux_guest_memory.c" "$here/../linux_guest_memory.c" "$here/../linux_boot.c" -o "$out"
ASAN_OPTIONS=detect_leaks=0 "$out" "$@"
cc -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -mgeneral-regs-only -mno-red-zone -Wframe-larger-than=1024 -c "$here/../linux_guest_memory.c" -o "$out.o"
