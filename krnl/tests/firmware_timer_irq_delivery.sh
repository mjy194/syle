#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
 -I"$here/.." "$here/../linux_external_irq.c" "$here/../firmware_timer_irq_owner.c" \
 "$here/../firmware_timer_irq_session.c" "$here/../firmware_irq_receipt.c" \
 "$here/../firmware_irq_native_evidence.c" "$here/../firmware_timer_irq_delivery.c" \
 "$here/firmware_timer_irq_delivery.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin -I"$here/.." \
 -c "$here/../firmware_timer_irq_delivery.c" -o "$tmp/model.o"
