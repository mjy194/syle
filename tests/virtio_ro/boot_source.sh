#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT HUP INT TERM
python3 boot_source_candidate.py ../../krnl/linux_boot.c "$tmp/candidate.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -g -I. -I../../krnl "$tmp/candidate.c" ../../krnl/linux_boot.c bounded_span.c boot_source_test.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
