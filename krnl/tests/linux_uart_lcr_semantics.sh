#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d /tmp/syle-lcr.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
for test in host-tests/linux_uart_lcr_semantics tests/linux_uart tests/linux_uart_model_io tests/linux_uart_runtime; do
 cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g "krnl/$test.c" krnl/linux_uart.c krnl/linux_uart_model_io.c krnl/linux_uart_runtime.c -o "$tmp/test"
 ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
done
cc -std=c11 -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/linux_uart.c -o "$tmp/uart.o"
