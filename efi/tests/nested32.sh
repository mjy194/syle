#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
./tests/nested1-policy.sh
cc -g -Wall -Wextra -Werror -DVMX_NESTED_TEST -fsanitize=address,undefined ../krnl/tests/nested_vcpu_policy.c ../krnl/nested_vcpu_policy.c -o build/nested-vcpu-policy-test
ASAN_OPTIONS=detect_leaks=0 build/nested-vcpu-policy-test
make vmx-nested-context vmx-nested-context-pf
for target in vmx-nested-context vmx-nested-context-pf; do
  ! nm "build/$target-esp/syle/krnl.elf" | grep -E ' (vmx_guest_regs|vmx_host_cr2|vmx_guest_cr2|guest_stack|vmx_guest_enter)$'
  strings "build/$target-esp/syle/krnl.elf" >"build/$target-strings.log"
  ! grep -q "VMX.ROOT_TEST_DISABLED" "build/$target-strings.log"
  nm "build/$target-esp/syle/krnl.elf" >"build/$target-symbols.log"
  grep -q " T nested_vcpu_enter$" "build/$target-symbols.log"
  objdump -dr build/k-nested32-probe.o >build/nested32-probe-disassembly.log
  grep -q "vmx_guest_run_context" build/nested32-probe-disassembly.log
  set +e
  ESP_DIR="build/$target-esp" timeout 90 scripts/run.sh smoke >"build/$target-tcg.log" 2>&1
  status=$?
  set -e
  [[ $status == 33 ]]
  grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' "build/$target-tcg.log"
  ! grep -q 'N32A_CONTEXT_PASS\|PANIC' "build/$target-tcg.log"
done
echo 'PASS N32A explicit context ABI/policy/build/TCG skip; hardware not tested'
