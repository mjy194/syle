#!/bin/sh
set -eu
# This no-allocation fixture is run under ptrace, where LSan is unavailable.
export ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:+$UBSAN_OPTIONS:}halt_on_error=1"
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc=${CC:-cc}
$cc -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -no-pie tests/guest_uart.c guest_uart.c -o "$tmp/uart"
"$tmp/uart"
$cc -std=c11 -Wall -Wextra -Werror -ffreestanding -fsyntax-only guest_uart.c
