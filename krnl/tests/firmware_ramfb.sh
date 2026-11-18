#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -DFFC_RAMFB -DFFC_NO_KERNEL_BLOBS "$here/firmware_ramfb.c" "$here/../firmware_ramfb.c" "$here/../ramfb_config.c" "$here/../firmware_fwcfg.c" -o "$build/test"
ASAN_OPTIONS=detect_leaks=0 "$build/test"
