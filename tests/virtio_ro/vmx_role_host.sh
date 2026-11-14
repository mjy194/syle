#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -DPMM_TEST_PHYS_BITS=36 -DPNA_HOST_TEST -DVLM_SLOTS=2 -std=c11 -O2 -Wall -Wextra -Werror -fno-lto -fsanitize=address,undefined -g -c native_candidate.c -o "$tmp/raw.o"
nm -g --defined-only "$tmp/raw.o" | awk '$3 ~ /^(pmm_raw_|pmm_owned_|vlm_)/ {print $3}' > "$tmp/private"
objcopy --localize-symbols="$tmp/private" "$tmp/raw.o" "$tmp/native.o"
cc -DPNA_HOST_TEST -DVLM_SLOTS=2 -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined -g vmx_role_host_test.c ../../krnl/linux_vcpu_host.c ../../krnl/linux_vcpu_prepare.c "$tmp/native.o" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
