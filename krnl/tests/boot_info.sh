#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p tests/build
${CC:-cc} -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer tests/boot_info.c -o tests/build/boot-info
ASAN_OPTIONS=detect_leaks=0 tests/build/boot-info
