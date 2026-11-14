#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer firmware_ram_adapter.c firmware_disk_test.c backend.c transport.c ../../krnl/firmware_memory.c ../../krnl/firmware_layout.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
