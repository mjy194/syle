#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I"$here/.." "$here/../firmware_observer_compact.c" "$here/firmware_observer_compact_replay.c" -o "$tmp/replay"
for f in run30-replay nodma4096-prefix-replay; do
 ASAN_OPTIONS=detect_leaks=0 "$tmp/replay" "$here/fixtures/$f.tape" "$here/fixtures/$f.text"
done
