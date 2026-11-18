#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -DSYLE_PIC_IRQ5 "$here/pic_irq5.c" "$here/../pic_master_04.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
