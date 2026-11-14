#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
src='krnl/linux_pic04_domain.c krnl/pit0.c krnl/pic_master_04.c krnl/linux_pic_mask.c krnl/linux_uart_runtime.c krnl/linux_uart_model_io.c krnl/linux_uart.c'
${CC:-cc} -std=c11 -Ikrnl -Wall -Wextra -Werror -fsanitize=address,undefined -g krnl/host-tests/linux_pic04_uart_clock.c $src -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Ikrnl -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/linux_pic04_domain.c -o "$tmp/domain.o"
