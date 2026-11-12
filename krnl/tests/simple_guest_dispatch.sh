#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p tests/build
${CC:-cc} -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer tests/simple_guest_dispatch.c simple_guest_dispatch.c simple_guest_image.c vmexit.c -o tests/build/simple-guest-dispatch
ASAN_OPTIONS=detect_leaks=0 tests/build/simple-guest-dispatch
