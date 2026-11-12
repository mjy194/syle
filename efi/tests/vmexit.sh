#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/vmexit.c ../krnl/vmexit.c -o build/vmexit-policy-test
ASAN_OPTIONS=detect_leaks=0 build/vmexit-policy-test
make vmx-exits
set +e
ESP_DIR=build/vmx-exits-esp timeout 90 scripts/run.sh smoke > build/vmexit-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/vmexit-tcg.log
! grep -q 'VMEXIT.DIAGNOSTIC_PASS\|VMX.ROOT_ENTERED\|PANIC' build/vmexit-tcg.log
echo 'PASS exit policy/compile/TCG skip; CPUID/HLT hardware execution not tested'
