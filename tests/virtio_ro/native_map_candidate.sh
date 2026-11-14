#!/bin/sh
set -eu
cd "$(dirname "$0")"
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT HUP INT TERM
flags='-DPMM_TEST_PHYS_BITS=36 -DPNA_HOST_TEST -std=c11 -O1 -Wall -Wextra -Werror -fno-lto -fsanitize=address,undefined -g'
cc $flags -c native_candidate.c -o "$t/raw.o"
nm -g --defined-only "$t/raw.o" | awk '$3 ~ /^(pmm_raw_|pmm_owned_|vlm_)/ {print $3}' > "$t/private"
objcopy --localize-symbols="$t/private" "$t/raw.o" "$t/native.o"
efi=/workspace/shared/syle-deps/root/usr/include/efi
cc $flags -I"$efi" -I"$efi/x86_64" -DVNMAP_HOST_TEST -DVMX_MEMORY_POLICY_ONLY native_map_candidate.c native_map_candidate_test.c ../../krnl/vmx_memory.c scoped_access.c session_authority.c create_context.c "$t/native.o" -o "$t/test"
ASAN_OPTIONS=detect_leaks=0 "$t/test"
for failure in read-only pat mtrr wrongpa absent la57 user; do ASAN_OPTIONS=detect_leaks=0 "$t/test" "$failure"; done
