#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/vmx_policy.c ../krnl/vmx_policy.c -o build/vmx-policy-test
ASAN_OPTIONS=detect_leaks=0 build/vmx-policy-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined -DVMX_MEMORY_POLICY_ONLY ../krnl/tests/vmx_memory.c ../krnl/vmx_memory.c -o build/vmx-memory-test
ASAN_OPTIONS=detect_leaks=0 build/vmx-memory-test
make vmx
set +e
ESP_DIR=build/vmx-esp timeout 90 scripts/run.sh smoke > build/vmx-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/vmx-tcg.log
grep -q 'STAGE0.SMOKE_PASS' build/vmx-tcg.log
! grep -q 'VMX.ROOT_ENTERED\|VMX.ROOT_ROUNDTRIP_PASS\|PANIC' build/vmx-tcg.log
echo 'PASS: policy cases and TCG unavailable path only; VMX instructions NOT tested'
