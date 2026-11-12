#!/bin/sh
# Host-only public PMM API calls; no physical address is dereferenced.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d /tmp/syle-pmm-model.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -DPMM_TEST_PHYS_BITS=48 -std=c11 -g -O1 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
    memory.c tests/memory_model.c -o "$tmp/test"
for seed in 1 2 3 42 65537 305419896 2654435769 4294967295; do
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$tmp/test" "$seed"
done
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$tmp/test" split-capacity
