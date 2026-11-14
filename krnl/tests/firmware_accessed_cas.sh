#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined "$here/../firmware_accessed_cas.c" "$here/firmware_accessed_cas.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin -c "$here/../firmware_accessed_cas.c" -o "$tmp/cas.o"
test -z "$(nm -u "$tmp/cas.o")"
objdump -d "$tmp/cas.o" | grep -q 'lock cmpxchg'
