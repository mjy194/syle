#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/exit_services.c -o build/exit-services-test
ASAN_OPTIONS=detect_leaks=0 ./build/exit-services-test
