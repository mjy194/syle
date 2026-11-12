#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/vcpu.c ../krnl/vcpu.c -o build/vcpu-policy-test
ASAN_OPTIONS=detect_leaks=0 build/vcpu-policy-test
make vmx-events
set +e
ESP_DIR=build/vmx-events-esp timeout 90 scripts/run.sh smoke > build/vcpu-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/vcpu-tcg.log
! grep -q 'VCPU.EVENT_DIAGNOSTIC_PASS\|VMX.ROOT_ENTERED\|PANIC' build/vcpu-tcg.log
echo 'PASS exit policy/compile/TCG skip; exception injection hardware execution not tested'
