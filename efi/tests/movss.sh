#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/lapic.c ../krnl/lapic.c -o build/lapic-policy-test
ASAN_OPTIONS=detect_leaks=0 build/lapic-policy-test
make vmx-movss
set +e
ESP_DIR=build/vmx-movss-esp timeout 90 scripts/run.sh smoke > build/movss-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/movss-tcg.log
! grep -q 'APIC.DIAGNOSTIC_PASS\|VMX.ROOT_ENTERED\|PANIC' build/movss-tcg.log
echo 'PASS exit policy/compile/TCG skip; active interrupt-window hardware execution not tested'
