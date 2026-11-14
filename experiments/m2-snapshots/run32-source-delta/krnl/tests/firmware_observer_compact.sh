#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I"$here/.." "$here/../firmware_observer_compact.c" "$here/firmware_observer_compact.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
cc -std=c11 -Wall -Wextra -Werror -O2 -ffreestanding -fno-builtin -c -I"$here/.." "$here/../firmware_observer_compact.c" -o "$tmp/model.o"
cc -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I"$here/.." "$here/../firmware_observer_compact.c" "$here/firmware_observer_compact_stress.c" -o "$tmp/stress"
ASAN_OPTIONS=detect_leaks=0 "$tmp/stress"
