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
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/cache_rendezvous.c ../krnl/cache_rendezvous.c -o build/cache-rendezvous-test
ASAN_OPTIONS=detect_leaks=0 build/cache-rendezvous-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/cache_mp_transition.c ../krnl/cache_mp_transition.c -o build/cache-mp-transition-test
ASAN_OPTIONS=detect_leaks=0 build/cache-mp-transition-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/cache_ept_alias.c ../krnl/cache_ept_alias.c -o build/cache-ept-alias-test
ASAN_OPTIONS=detect_leaks=0 build/cache-ept-alias-test
make smp-cache-transition
set +e
QEMU_CPUS=2 ESP_DIR=build/smp-cache-transition-esp timeout 90 scripts/run.sh smoke >build/smp-cache-transition-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'SMP.N20_BLOCKED CPUID_VMX0' build/smp-cache-transition-tcg.log
! grep -q 'SMP.N31B_PASS\|PANIC' build/smp-cache-transition-tcg.log
echo 'PASS N31B build/TCG capability rejection; two-CPU VMX requires KVM'
