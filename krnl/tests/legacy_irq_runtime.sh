#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Ikrnl -Wall -Wextra -Werror -fsanitize=address,undefined -g krnl/host-tests/legacy_irq_runtime.c krnl/legacy_irq_runtime.c krnl/pit0.c krnl/pic0.c krnl/pit0_transaction.c krnl/pic0_glue.c krnl/linux_external_irq.c krnl/guest_wait.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Ikrnl -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/legacy_irq_runtime.c -o "$tmp/model.o"
