#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -DLRTC_FIRMWARE_COLD_BOOT "$here/../firmware_cmos.c" "$here/../firmware_cmos_memory.c" "$here/../firmware_layout.c" "$here/../linux_rtc.c" "$here/firmware_cmos.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
cat > "$tmp/default.c" <<'SRC'
#include "linux_rtc.h"
#include <assert.h>
static bool wr(void*p,uint32_t f,uint64_t v){(void)p;(void)f;(void)v;return true;}
int main(void){struct lrtc_state s={0};uint64_t g[15]={15};g[3]=0x70;assert(lrtc_io(&s,g,0x1000,0,30,0x700000,1,0,0,wr));g[3]=0x71;assert(!lrtc_io(&s,g,0x1001,0,30,0x710008,1,0,0,wr));}
SRC
${CC:-cc} -std=c11 -Wall -Wextra -Werror -I"$here/.." "$tmp/default.c" "$here/../linux_rtc.c" -o "$tmp/default"
"$tmp/default"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -fno-builtin -c "$here/../firmware_cmos.c" -o "$tmp/model.o"
