#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -DPMM_TEST_PHYS_BITS=36 -DPNA_HOST_TEST -std=c11 -O2 -Wall -Wextra -Werror -fno-lto -fsanitize=address,undefined -g -c native_candidate.c -o "$tmp/raw.o"
nm -g --defined-only "$tmp/raw.o" | awk '$3 ~ /^(pmm_raw_|pmm_owned_|vlm_)/ {print $3}' > "$tmp/private"
objcopy --localize-symbols="$tmp/private" "$tmp/raw.o" "$tmp/native.o"
cc -DPNA_HOST_TEST -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined -g session_scope.c session_scope_test.c "$tmp/native.o" -Wl,--wrap=pna_unpin -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
for mode in partial drain unpin reject-unpin fatal-source fatal-transfer;do ASAN_OPTIONS=detect_leaks=0 "$tmp/test" "$mode";done
