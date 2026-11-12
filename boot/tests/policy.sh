#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer boot.c config.c tests/policy.c -o build/policy-test
ASAN_OPTIONS=detect_leaks=0 ./build/policy-test
