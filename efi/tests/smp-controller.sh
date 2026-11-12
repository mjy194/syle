#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -DSMP_VMX_POLICY_ONLY -fsanitize=address,undefined ../krnl/tests/smp_vmx.c ../krnl/smp_vmx.c -o build/smp-vmx-policy-test
ASAN_OPTIONS=detect_leaks=0 build/smp-vmx-policy-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/smp_guest.c -o build/smp-guest-policy-test
ASAN_OPTIONS=detect_leaks=0 build/smp-guest-policy-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/shootdown.c ../krnl/shootdown.c -o build/shootdown-test
ASAN_OPTIONS=detect_leaks=0 build/shootdown-test
make smp-controller
set +e
QEMU_CPUS=2 ESP_DIR=build/smp-controller-esp timeout 90 scripts/run.sh smoke >build/smp-controller-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'SMP.N20_BLOCKED CPUID_VMX0' build/smp-controller-tcg.log
! grep -q 'SMP.N23_PASS\|PANIC' build/smp-controller-tcg.log
echo 'PASS N23 build/TCG capability rejection; two-CPU VMX requires KVM'
