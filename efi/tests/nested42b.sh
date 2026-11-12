#!/usr/bin/env bash
# Focused host checks; explicit runtime mode builds and checks TCG capability skip.
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../.."
mode=${1:---host}
case "$mode" in --host|host) mode=host;; --runtime|runtime) mode=runtime;; all) ;; *) echo 'Usage: nested42b.sh [--host|--runtime|all]' >&2; exit 2;; esac
source efi/scripts/cloud-env.sh
out=$(mktemp -d /tmp/syle-nested42b-host.XXXXXX)
echo "N42B host evidence: $out"
if [[ $mode != runtime ]]; then
sources=(nested_owner_atomic_maintenance_test.c nested_atomic_retrack.c nested_owner_atomic_maintenance.c nested_owner_atomic.c nested_atomic_memory.c nested_domain_memory.c nested_vcpu_policy.c nested_domain_dispatch.c nested_domain.c nested_vmx.c typed_root_atomic.c typed_root_registry.c typed_root_lease.c typed_root_local_provider.c typed_directory_registry.c typed_directory.c nested_atomic_catalog.c nested_root_catalog.c nested_ept_atomic.c ad_updater_gate.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c nested_ept.c)
for i in "${!sources[@]}"; do sources[$i]=krnl/${sources[$i]}; done
cc -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -Ikrnl -I"${EFI_INC:-$EFI_PREFIX/include/efi}" -I"${EFI_INC:-$EFI_PREFIX/include/efi}/x86_64" -DVMX_NESTED_DOMAIN_TEST -DVMX_NESTED_OWNER_SPARSE_TEST -DVMX_NESTED_OWNER_CONTEXT_TEST -DVMX_NESTED_ATOMIC_CATALOG_TEST "${sources[@]}" -o "$out/runtime"
ASAN_OPTIONS=detect_leaks=0 timeout 90 "$out/runtime" 2>&1 | tee "$out/runtime.log"

cc -std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined -DNESTED_PARALLEL_POLICY_ONLY -DVMX_NESTED_DOMAIN_TEST -DVMX_NESTED_OWNER_SPARSE_TEST -DVMX_NESTED_OWNER_CONTEXT_TEST -DVMX_NESTED_ATOMIC_CATALOG_TEST -DVMX_NESTED_ATOMIC_RETRACK_TEST -Ikrnl krnl/nested_atomic_retrack_fault_test.c krnl/nested_parallel.c -o "$out/fault"
ASAN_OPTIONS=detect_leaks=0 "$out/fault" | tee "$out/fault.log"
python3 efi/tests/nested42b_integration.py | tee "$out/integration.log"
python3 efi/tests/nested42b_independent_review.py . --efi-prefix "$EFI_PREFIX" | tee "$out/independent.log"
python3 efi/tests/nested42b_freestanding.py | tee "$out/freestanding.log"
echo 'PASS N42B focused production host and freestanding wiring; physical primitives mocked, no VMX claim'
fi
if [[ $mode != host ]]; then
 make -C efi smp-nested-atomic-retrack
 nm -u efi/build/krnl-smp-nested-atomic-retrack.elf > "$out/undefined"
 [[ ! -s $out/undefined ]]
 nm efi/build/krnl-smp-nested-atomic-retrack.elf > "$out/symbols"
 for symbol in noam_bind noam_remove noam_restore noam_flush nart_round tam_close tam_clear tam_reset tam_prepare tam_finish nested_domain_dispatch_maintenance_stopped; do
  grep -Eq " T $symbol$" "$out/symbols"
 done
 set +e
 QEMU_CPUS=2 ESP_DIR=build/smp-nested-atomic-retrack-esp timeout 90 efi/scripts/run.sh smoke > "$out/tcg.log" 2>&1
 status=$?
 set -e
 [[ $status == 33 ]]
 grep -Fq 'SMP.N20_BLOCKED CPUID_VMX0' "$out/tcg.log"
 ! grep -Eq 'SMP.N42B_PASS|N42B_CHECKPOINT=|PANIC' "$out/tcg.log"
 echo 'PASS N42B TCG CPUID_VMX0 capability skip; exact-image VMX clear/retrack requires separate hardware acceptance'
fi
