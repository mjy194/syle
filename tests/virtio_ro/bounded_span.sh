#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -g bounded_span.c bounded_span_test.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
