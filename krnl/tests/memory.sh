#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p tests/build
${CC:-cc} -DPMM_TEST_PHYS_BITS=36 -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer memory.c tests/memory.c -o tests/build/memory
ASAN_OPTIONS=detect_leaks=0 tests/build/memory
ASAN_OPTIONS=detect_leaks=0 tests/build/memory fragmentation
ASAN_OPTIONS=detect_leaks=0 tests/build/memory zero
ASAN_OPTIONS=detect_leaks=0 tests/build/memory single
for bits in 48 53; do
    ${CC:-cc} -DPMM_TEST_PHYS_BITS=$bits -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer memory.c tests/memory.c -o tests/build/memory-$bits
done
ASAN_OPTIONS=detect_leaks=0 tests/build/memory-48 cap
ASAN_OPTIONS=detect_leaks=0 tests/build/memory-48 sparse
ASAN_OPTIONS=detect_leaks=0 tests/build/memory-53 cpu
