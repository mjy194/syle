#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -DPMM_TEST_PHYS_BITS=36 -DPNA_HOST_TEST -DVLM_SLOTS=4 -std=c11 -Wall -Wextra -Werror -pedantic -fno-lto -fsanitize=address,undefined -g -c native_candidate.c -o "$tmp/raw.o"
nm -g --defined-only "$tmp/raw.o" | awk '$3 ~ /^(pmm_raw_|pmm_owned_|vlm_)/ {print $3}' > "$tmp/private"
objcopy --localize-symbols="$tmp/private" "$tmp/raw.o" "$tmp/native.o"
cc -std=c11 -Wall -Wextra -Werror -pedantic -ffreestanding -mgeneral-regs-only -mno-red-zone -c deferred_context.c -o "$tmp/context.o"
cc -DPNA_HOST_TEST -DVLM_SLOTS=4 -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g deferred_context.c deferred_context_test.c "$tmp/native.o" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g deferred_context.c deferred_context_failure_test.c -o "$tmp/failure"
ASAN_OPTIONS=detect_leaks=0 "$tmp/failure"
