#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
cc=${CC:-cc}
"$cc" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  "$root/krnl/guest_mmio_mov.c" "$root/krnl/tests/guest_mmio_mov.c" -o "$build/test"
ASAN_OPTIONS=detect_leaks=0 "$build/test"
"$cc" -std=c11 -Wall -Wextra -Werror -ffreestanding -mno-red-zone \
  -c "$root/krnl/guest_mmio_mov.c" -o "$build/model.o"
