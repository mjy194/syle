#!/bin/sh
set -eu
cd "$(dirname "$0")"
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT HUP INT TERM
flags='-DPMM_TEST_PHYS_BITS=36 -DPNA_HOST_TEST -std=c11 -O1 -Wall -Wextra -Werror -fno-lto -fsanitize=address,undefined -g'
cc $flags -c native_candidate.c -o "$t/raw.o"
nm -g --defined-only "$t/raw.o" | awk '$3 ~ /^(pmm_raw_|pmm_owned_|vlm_)/ {print $3}' > "$t/private"
objcopy --localize-symbols="$t/private" "$t/raw.o" "$t/native.o"
cc $flags session_authority.c create_context.c session_authority_test.c "$t/native.o" -o "$t/test"
ASAN_OPTIONS=detect_leaks=0 "$t/test"
cc $flags -Dpna_page=fi_pna_page -c create_context.c -o "$t/create-fi.o"
cc $flags ../../krnl/guest_control.c ../../krnl/linux_guest_session.c ../../krnl/linux_guest_role_binding.c session_authority.c "$t/create-fi.o" session_epoch_test.c "$t/native.o" -o "$t/epoch-test"
ASAN_OPTIONS=detect_leaks=0 "$t/epoch-test"
ASAN_OPTIONS=detect_leaks=0 "$t/epoch-test" fail-admission
