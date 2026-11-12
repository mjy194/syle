#!/bin/sh
set -eu
# LeakSanitizer cannot run under ptrace; the fixture counts every live allocation.
export ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:+$UBSAN_OPTIONS:}halt_on_error=1"
# Resolve the optional artifact before changing directories.
if [ "$#" -gt 0 ]; then
    artifact=$(realpath "$1")
    set -- "$artifact"
fi
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc=${CC:-cc}
flags='-std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie'
$cc $flags tests/simple_guest_image.c simple_guest_image.c -o "$tmp/image"
$cc $flags tests/simple_guest_memory.c simple_guest_image.c simple_guest_memory.c -o "$tmp/memory"
"$tmp/image"
"$tmp/memory" "$@"
