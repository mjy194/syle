#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./tests/nested1-policy.sh
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_events.c ../krnl/nested_events.c -o build/nested-gp-inject-test
ASAN_OPTIONS=detect_leaks=0 build/nested-gp-inject-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_entry.c ../krnl/nested_events.c ../krnl/nested_vmx.c -o build/nested-entry-test
ASAN_OPTIONS=detect_leaks=0 build/nested-entry-test
make vmx-nested-gp-inject
set +e
ESP_DIR=build/vmx-nested-gp-inject-esp timeout 90 scripts/run.sh smoke >build/nested7-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/nested7-tcg.log
! grep -q 'NESTED.N7_PASS\|VMX.ROOT_ENTERED\|PANIC' build/nested7-tcg.log
echo 'PASS N7 pure policies/build/TCG skip; real nesting not tested'
