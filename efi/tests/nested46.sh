#!/usr/bin/env bash
# Combined fixed UC AD1 software gates or explicit TCG capability skip. Never KVM.
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../.."
mode=${1:---host}
case "$mode" in --host|host) mode=host;; --runtime|runtime) mode=runtime;; all) ;; *) echo 'Usage: nested46.sh [--host|--runtime|all]' >&2; exit 2;; esac
source efi/scripts/cloud-env.sh
out=$(mktemp -d /tmp/syle-nested46-check.XXXXXX)
echo "N46 evidence: $out"
if [[ $mode != runtime ]]; then
 for sanitizer in none address,undefined thread; do
  for gate in nested46a_core nested46b_memory nested46b_provision nested46b_runtime; do
   python3 "efi/tests/$gate.py" --sanitizer "$sanitizer" | tee "$out/$gate-$sanitizer.log"
  done
 done
 python3 efi/tests/nested46a_independent.py . | tee "$out/independent-core.log"
 python3 efi/tests/nested46b_independent.py . | tee "$out/independent-adapters.log"
 python3 efi/tests/nested46b_runtime_independent.py . | tee "$out/independent-runtime.log"
 python3 efi/tests/nested46b_integration.py | tee "$out/integration.log"
 python3 efi/tests/nested46b_freestanding.py | tee "$out/freestanding.log"
 echo 'PASS N46 combined focused software gates; physical primitives mocked, no physical UC or VMX claim'
fi
if [[ $mode != host ]]; then
 make -C efi smp-nested-atomic-uc > "$out/build.log" 2>&1
 nm -u efi/build/krnl-smp-nested-atomic-uc.elf > "$out/undefined"
 [[ ! -s $out/undefined ]]
 cmp efi/build/krnl-smp-nested-atomic-uc.elf efi/build/smp-nested-atomic-uc-esp/syle/krnl.elf
 sha256sum efi/build/krnl-smp-nested-atomic-uc.elf > "$out/image-sha256.txt"
 set +e
 QEMU_CPUS=2 timeout 90 efi/scripts/run.sh smp-nested-atomic-uc > "$out/tcg.log" 2>&1
 status=$?
 set -e
 [[ $status == 33 ]]
 grep -Fq 'SMP.N20_BLOCKED CPUID_VMX0' "$out/tcg.log"
 ! grep -Eq 'SMP.N46B_PASS|N46B_CHECKPOINT=|N46B_PROVISION_ORDINAL=|PANIC' "$out/tcg.log"
 echo 'PASS N46 TCG CPUID_VMX0 capability skip; no physical UC or VMX execution claim'
fi
