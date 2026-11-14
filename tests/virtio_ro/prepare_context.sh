#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT HUP INT TERM
python3 prepare_context_candidate.py ../../krnl/linux_guest_manager.c "$tmp/manager.c" "$tmp/prepare_fragment.inc"
cc -std=c11 -Wall -Wextra -Werror -DPREPARE_OBSERVE_HOST_TEST -Dsource_prepare_observe=actual_source_prepare_observe -fsanitize=address,undefined -c prepare_context_observe.c -o "$tmp/observe.o"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I../../krnl -I"$tmp" prepare_context_test.c "$tmp/observe.o" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
cc -O1 -ffreestanding -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-pic -fno-pie -c prepare_context_observe.c -o "$tmp/native.o"
cc -c prepare_context_pause.S -o "$tmp/pause.o"
objdump -dr "$tmp/native.o" "$tmp/pause.o"
