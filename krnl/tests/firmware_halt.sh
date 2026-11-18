#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -DSYLE_FMM_DE_CONFIG \
 "$here/../firmware_layout.c" "$here/../firmware_memory.c" "$here/../firmware_mmio_memory.c" \
 "$here/../firmware_halt.c" "$here/../linux_external_irq.c" "$here/../firmware_timer_irq_owner.c" \
 "$here/../firmware_timer_irq_session.c" "$here/../firmware_irq_receipt.c" \
 "$here/../firmware_irq_native_evidence.c" "$here/../firmware_timer_irq_delivery.c" \
 "$here/firmware_halt.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin \
 -c "$here/../firmware_halt.c" -o "$tmp/halt.o"
