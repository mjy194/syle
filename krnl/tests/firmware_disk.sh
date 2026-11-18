#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined "$here/firmware_disk.c" "$here/../virtio_blk/backend.c" "$here/../virtio_blk/transport.c" "$here/../virtio_blk/firmware_ram_adapter.c" "$here/../firmware_memory.c" "$here/../firmware_layout.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
