#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./tests/nested1-policy.sh
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_failure.c ../krnl/nested_vmx.c ../krnl/nested_profile.c -o build/nested-failure-test
ASAN_OPTIONS=detect_leaks=0 build/nested-failure-test
make vmx-nested-failure
set +e
ESP_DIR=build/vmx-nested-failure-esp timeout 90 scripts/run.sh smoke >build/nested4-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/nested4-tcg.log
! grep -q 'NESTED.N4_PASS\|VMX.ROOT_ENTERED\|PANIC' build/nested4-tcg.log
echo 'PASS N4 pure policies/build/TCG skip; real nesting not tested'
