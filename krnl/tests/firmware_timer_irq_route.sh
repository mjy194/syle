#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
 "$here/../firmware_timer_irq_owner.c" "$here/../firmware_timer_100m.c" \
 "$here/../firmware_timer_irq_import.c" "$here/../firmware_timer_irq_route.c" "$here/firmware_timer_irq_route.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
 -c "$here/../firmware_timer_irq_route.c" -o "$tmp/model.o"
