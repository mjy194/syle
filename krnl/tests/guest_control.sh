#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer guest_control.c tests/guest_control.c -o "$build/test"
ASAN_OPTIONS=detect_leaks=0 "$build/test"
