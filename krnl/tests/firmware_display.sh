#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined "$here/firmware_display.c" "$here/../firmware_display.c" -o "$build/test"
ASAN_OPTIONS=detect_leaks=0 "$build/test"
