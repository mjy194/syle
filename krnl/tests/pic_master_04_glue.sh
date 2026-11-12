#!/bin/sh
# Host bridge only; no runtime integration or VM.
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Ikrnl krnl/pic_master_04_glue.c krnl/pic_master_04.c krnl/linux_external_irq.c krnl/host-tests/pic_master_04_glue.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -Ikrnl -c krnl/pic_master_04_glue.c -o "$tmp/model.o"
