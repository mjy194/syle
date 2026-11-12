#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/lapic_timer.c ../krnl/lapic_timer.c -o build/timer-policy-test
ASAN_OPTIONS=detect_leaks=0 build/timer-policy-test
make vmx-periodic
set +e
ESP_DIR=build/vmx-periodic-esp timeout 90 scripts/run.sh smoke > build/periodic-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/periodic-tcg.log
! grep -q 'APIC.PERIODIC_DIAGNOSTIC_PASS\|VMX.ROOT_ENTERED\|PANIC' build/periodic-tcg.log
echo 'PASS exit policy/compile/TCG skip; x2APIC subset hardware execution not tested'
