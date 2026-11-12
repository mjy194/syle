#!/usr/bin/env bash
# Focused N44 software gates or an explicit TCG capability skip. Never KVM.
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../.."
mode=${1:---host}
case "$mode" in --host|host) mode=host;; --runtime|runtime) mode=runtime;; all) ;; *) echo 'Usage: nested44.sh [--host|--runtime|all]' >&2; exit 2;; esac
source efi/scripts/cloud-env.sh
out=$(mktemp -d /tmp/syle-nested44-check.XXXXXX)
echo "N44 evidence: $out"
if [[ $mode != runtime ]]; then
 for sanitizer in none address,undefined thread; do
  python3 efi/tests/nested44_core.py --sanitizer "$sanitizer" | tee "$out/core-$sanitizer.log"
  python3 efi/tests/nested44_memory.py --sanitizer "$sanitizer" | tee "$out/memory-$sanitizer.log"
 done
 python3 efi/tests/nested44_independent_review.py . --efi-prefix "$EFI_PREFIX" | tee "$out/independent.log"
 python3 efi/tests/nested44_integration.py | tee "$out/integration.log"
 python3 efi/tests/nested44_freestanding.py | tee "$out/freestanding.log"
 echo 'PASS N44 focused production/independent/structural/freestanding gates; physical primitives mocked, no VMX claim'
fi
if [[ $mode != host ]]; then
 make -C efi smp-nested-atomic-largepage > "$out/build.log" 2>&1
 nm -u efi/build/krnl-smp-nested-atomic-largepage.elf > "$out/undefined"
 [[ ! -s $out/undefined ]]
 nm efi/build/krnl-smp-nested-atomic-largepage.elf > "$out/symbols"
 for symbol in noam_commit namr_round tam_close tam_clear tam_reset tam_prepare tam_finish; do
  grep -Eq " T $symbol$" "$out/symbols"
 done
 for n in {0..12}; do grep -Eq " [RrTt] nested_atomic_multiround_call$n$" "$out/symbols"; done
 cmp efi/build/krnl-smp-nested-atomic-largepage.elf efi/build/smp-nested-atomic-largepage-esp/syle/krnl.elf
 sha256sum efi/build/krnl-smp-nested-atomic-largepage.elf > "$out/image-sha256.txt"
 set +e
 QEMU_CPUS=2 timeout 90 efi/scripts/run.sh smp-nested-atomic-largepage > "$out/tcg.log" 2>&1
 status=$?
 set -e
 [[ $status == 33 ]]
 grep -Fq 'SMP.N20_BLOCKED CPUID_VMX0' "$out/tcg.log"
 ! grep -Eq 'SMP.N44_PASS|N44_CHECKPOINT=|PANIC' "$out/tcg.log"
 echo 'PASS N44 TCG CPUID_VMX0 capability skip; no VMX two-round execution claim'
fi
