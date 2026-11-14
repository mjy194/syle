#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
 "$here/../firmware_layout.c" "$here/../firmware_memory.c" "$here/../firmware_mmio_memory.c" "$here/../guest_mmio_mov.c" "$here/../firmware_tpm_absent.c" "$here/../firmware_tpm_transaction.c" "$here/firmware_tpm_transaction.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
 -c "$here/../firmware_tpm_transaction.c" -o "$tmp/memory.o"
