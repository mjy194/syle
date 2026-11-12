#!/usr/bin/env bash
# TCG is a negative-path test, never hardware validation.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/vmx_guest.c -o build/vmx-guest-policy-test
ASAN_OPTIONS=detect_leaks=0 build/vmx-guest-policy-test
make vmx-guest
set +e
ESP_DIR=build/vmx-guest-esp timeout 90 scripts/run.sh smoke > build/vmx-guest-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/vmx-guest-tcg.log
grep -q 'STAGE0.SMOKE_PASS' build/vmx-guest-tcg.log
! grep -q 'VMX.ROOT_ENTERED\|VMX.GUEST_VMCALL_PASS\|PANIC' build/vmx-guest-tcg.log
echo 'PASS: guest diagnostic compiles and safely skips on TCG; hardware guest execution NOT tested'
