#!/bin/sh
set -eu
export ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:+$UBSAN_OPTIONS:}halt_on_error=1"
cd "$(dirname "$0")/.."
mkdir -p tests/build
${CC:-cc} -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie tests/simple_guest_io.c simple_guest_io.c guest_uart.c simple_guest_dispatch.c simple_guest_image.c vmexit.c -o tests/build/simple-guest-io
tests/build/simple-guest-io
