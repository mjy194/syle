#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined "$here/virtio_rw.c" "$here/../virtio_blk/rw_backend.c" "$here/../virtio_blk/rw_transport.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
