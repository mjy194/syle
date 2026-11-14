#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
for flags in "" "-DLRTC_FIRMWARE_COLD_BOOT"; do
 ${CC:-cc} $flags -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
 "$here/../firmware_layout.c" "$here/../firmware_cmos_memory.c" "$here/../linux_rtc.c" "$here/../firmware_cmos.c" "$here/../firmware_cmos_index_read.c" "$here/firmware_cmos_index_read.c" -o "$tmp/test"
 ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
done
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin -c "$here/../firmware_cmos_index_read.c" -o "$tmp/model.o"
