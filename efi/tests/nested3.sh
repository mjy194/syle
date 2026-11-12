#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./tests/nested1-policy.sh
make vmx-nested-state
set +e
ESP_DIR=build/vmx-nested-state-esp timeout 90 scripts/run.sh smoke >build/nested3-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/nested3-tcg.log
! grep -q 'NESTED.N3_PASS\|VMX.ROOT_ENTERED\|PANIC' build/nested3-tcg.log
echo 'PASS N3 pure policies/build/TCG skip; real nesting not tested'
