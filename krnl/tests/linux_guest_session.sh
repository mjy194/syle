#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer linux_guest_session.c tests/linux_guest_session.c -o "$build/test"
ASAN_OPTIONS=detect_leaks=0 "$build/test"
