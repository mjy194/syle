#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./tests/nested1-policy.sh
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept.c ../krnl/nested_ept.c -o build/nested-ept-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept_map.c ../krnl/nested_ept_map.c ../krnl/nested_ept.c -o build/nested-ept-map-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-map-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined -DNESTED_MAP_HW_UNIT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -I"${EFI_PREFIX}/include/efi" -I"${EFI_PREFIX}/include/efi/x86_64" ../krnl/tests/nested_ept_map_hw.c ../krnl/nested_ept_map_hw.c ../krnl/nested_ept_map.c ../krnl/nested_ept.c -o build/nested-ept-map-hw-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-map-hw-test
make vmx-nested-ept-map
set +e
ESP_DIR=build/vmx-nested-ept-map-esp timeout 90 scripts/run.sh smoke >build/nested10-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/nested10-tcg.log
! grep -q 'NESTED.N10_PASS\|VMX.ROOT_ENTERED\|PANIC' build/nested10-tcg.log
echo 'PASS N10 pure policies/build/TCG skip; nested EPT hardware not tested'
