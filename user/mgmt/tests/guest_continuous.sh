#!/bin/sh
set -eu
cd "$(dirname "$0")"
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined guest_continuous.c -o "$build/test"
ASAN_OPTIONS=detect_leaks=0 "$build/test"
