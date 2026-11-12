#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./tests/nested1-policy.sh
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept.c ../krnl/nested_ept.c -o build/nested-ept-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-test
make vmx-nested-ept
set +e
ESP_DIR=build/vmx-nested-ept-esp timeout 90 scripts/run.sh smoke >build/nested2-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/nested2-tcg.log
! grep -q 'NESTED.N2_PASS\|VMX.ROOT_ENTERED\|PANIC' build/nested2-tcg.log
echo 'PASS N2 pure policies/build/TCG skip; nested EPT hardware not tested'
