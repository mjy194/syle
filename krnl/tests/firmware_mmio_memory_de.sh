#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
for profile in default de; do
 flags=""; if [ "$profile" = de ]; then flags=-DSYLE_FMM_DE_CONFIG; fi
 ${CC:-cc} -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined $flags \
 "$here/../firmware_layout.c" "$here/../firmware_memory.c" "$here/../firmware_mmio_memory.c" "$here/firmware_mmio_memory_de.c" -o "$tmp/$profile"
 ASAN_OPTIONS=detect_leaks=0 "$tmp/$profile"
done
