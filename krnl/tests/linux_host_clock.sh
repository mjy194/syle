#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
b=$(mktemp -d); trap 'rm -rf "$b"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address linux_host_clock.c tests/linux_host_clock.c -o "$b/test"
ASAN_OPTIONS=detect_leaks=0 "$b/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -c linux_host_clock.c -o "$b/clock.o"
if nm -u "$b/clock.o" | grep -E '__.*ti3'; then echo 'unexpected 128-bit runtime' >&2; exit 1; fi
src='linux_pic04_domain.c pit0.c pic_master_04.c linux_pic_mask.c linux_uart_runtime.c linux_uart_model_io.c linux_uart.c'
${CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address linux_host_clock.c tests/linux_host_clock_domain.c $src -o "$b/domain"
ASAN_OPTIONS=detect_leaks=0 "$b/domain"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address linux_host_clock_source.c tests/linux_host_clock_source.c -o "$b/source"
ASAN_OPTIONS=detect_leaks=0 "$b/source"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address linux_host_clock_hv.c tests/linux_host_clock_hv.c -o "$b/hv"
ASAN_OPTIONS=detect_leaks=0 "$b/hv"
