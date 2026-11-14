#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${MB_INPUTS:=/workspace/shared/clock-evidence/inputs}"
python3 ../../guest/linux-baseline/check-inputs.py --inputs "$MB_INPUTS" --manifest "$MB_INPUTS/manifest.json"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -DPMM_TEST_PHYS_BITS=36 -DPNA_HOST_TEST -std=c11 -O2 -Wall -Wextra -Werror -fno-lto -fsanitize=address,undefined -g -c native_candidate.c -o "$tmp/raw.o"
nm -g --defined-only "$tmp/raw.o" | awk '$3 ~ /^(pmm_raw_|pmm_owned_|vlm_)/ {print $3}' > "$tmp/private"
objcopy --localize-symbols="$tmp/private" "$tmp/raw.o" "$tmp/native.o"
cc -DPNA_HOST_TEST -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined -g lgm_typed_host_test.c ../../krnl/linux_guest_memory.c ../../krnl/linux_boot.c "$tmp/native.o" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test" "$MB_INPUTS/bzImage" "$MB_INPUTS/initramfs.cpio"
for failure in alloc map wb; do ASAN_OPTIONS=detect_leaks=0 "$tmp/test" "$MB_INPUTS/bzImage" "$MB_INPUTS/initramfs.cpio" "$failure"; done
