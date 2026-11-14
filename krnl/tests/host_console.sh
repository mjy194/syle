#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
inc=${EFI_INC:-${EFI_PREFIX:-/usr}/include/efi}
flags="-std=c11 -Wall -Wextra -Werror -g -DSYLE_KERNEL -ffunction-sections -fdata-sections -Wl,--gc-sections -fsanitize=address,undefined"
for opt in default terminal; do
 defs=;test "$opt" = default || defs=-DSYLE_HOST_UART_INPUT
 ${CC:-cc} $flags $defs -I"$inc" -I"$inc/x86_64" krnl/tests/host_console_syscall.c krnl/guest_control.c -o "$build/$opt"
 ASAN_OPTIONS=detect_leaks=0 "$build/$opt"
done
${CC:-cc} $flags -I"$inc" -I"$inc/x86_64" krnl/tests/host_uart_runtime.c -o "$build/runtime"
ASAN_OPTIONS=detect_leaks=0 "$build/runtime"
