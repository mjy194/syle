#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./tests/nested1-policy.sh
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_events.c ../krnl/nested_events.c -o build/nested-events-test
ASAN_OPTIONS=detect_leaks=0 build/nested-events-test
make vmx-nested-events
set +e
ESP_DIR=build/vmx-nested-events-esp timeout 90 scripts/run.sh smoke >build/nested5-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/nested5-tcg.log
! grep -q 'NESTED.N5_PASS\|VMX.ROOT_ENTERED\|PANIC' build/nested5-tcg.log
echo 'PASS N5 pure policies/build/TCG skip; real nesting not tested'
