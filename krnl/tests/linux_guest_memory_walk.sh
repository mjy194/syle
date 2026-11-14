#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined \
 "$here/linux_guest_memory_walk.c" "$here/../linux_guest_memory.c" "$here/../linux_boot.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
