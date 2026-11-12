#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -DSMP_VMX_POLICY_ONLY -fsanitize=address,undefined ../krnl/tests/smp_vmx.c ../krnl/smp_vmx.c -o build/smp-vmx-policy-test
ASAN_OPTIONS=detect_leaks=0 build/smp-vmx-policy-test
make smp-vmx
set +e
QEMU_CPUS=2 ESP_DIR=build/smp-vmx-esp timeout 90 scripts/run.sh smoke >build/smp-vmx-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'SMP.N20_BLOCKED CPUID_VMX0' build/smp-vmx-tcg.log
! grep -q 'SMP.N20_PASS\|PANIC' build/smp-vmx-tcg.log
echo 'PASS N20 build/TCG capability rejection; two-CPU VMX requires KVM'
