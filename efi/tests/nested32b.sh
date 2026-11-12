#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -DVMX_NESTED_TEST -fsanitize=address,undefined ../krnl/tests/nested_parallel.c ../krnl/nested_parallel.c -o build/nested-parallel-test
ASAN_OPTIONS=detect_leaks=0 build/nested-parallel-test
./tests/nested32.sh
./tests/ept.sh
make smp-nested
for obj in build/k-nested32b-guest.o build/k-nested32b-adapter.o; do
  nm -u "$obj" >"$obj.undefined"
  ! grep -E ' (pmm_alloc_page|pmm_free_page|log_text|log_hex|panic|vmx_guest_enter)$' "$obj.undefined"
done
nm build/smp-nested-esp/syle/krnl.elf >build/nested32b-symbols.log
grep -q ' T nested_vcpu_enter$' build/nested32b-symbols.log
grep -q ' [RT] nested_parallel_call$' build/nested32b-symbols.log
set +e
QEMU_CPUS=2 ESP_DIR=build/smp-nested-esp timeout 90 scripts/run.sh smoke >build/nested32b-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'SMP.N20_BLOCKED CPUID_VMX0' build/nested32b-tcg.log
! grep -q 'SMP.N32B_PASS\|PANIC' build/nested32b-tcg.log
echo 'PASS N32B isolated runtime/ownership/build/TCG skip; actual overlap requires two-CPU KVM'
