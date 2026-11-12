#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tests/nested36a.sh
tests/nested38a.sh
out=$(mktemp -d /tmp/syle-n38b.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined)
cc "${flags[@]}" ../krnl/tests/domain_typed_alias_plan.c ../krnl/domain_typed_alias_plan.c ../krnl/domain_typed_inventory.c -o "$out/aliases"
cc "${flags[@]}" -DVMX_NESTED_DOMAIN_TEST ../krnl/tests/nested_owner_typed.c ../krnl/nested_owner_typed.c ../krnl/nested_ept.c ../krnl/nested_ept_sparse.c ../krnl/nested_owner_pool.c ../krnl/domain_typed_lease.c -o "$out/owner"
cc "${flags[@]}" ../krnl/tests/cache_page.c ../krnl/cache_page.c -o "$out/host"
cc "${flags[@]}" ../krnl/tests/nested_domain_maintenance.c ../krnl/nested_domain.c ../krnl/nested_domain_dispatch.c ../krnl/nested_vmx.c -o "$out/gate"
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined -DVMX_NESTED_DOMAIN_TEST -I"${EFI_INC:-${EFI_PREFIX:-/usr}/include/efi}" -I"${EFI_INC:-${EFI_PREFIX:-/usr}/include/efi}/x86_64" ../krnl/tests/nested_domain_memory.c ../krnl/nested_domain_memory.c ../krnl/nested_ept.c -o "$out/setup"
cc "${flags[@]}" -DVMX_NESTED_DOMAIN_TEST -DVMX_NESTED_OWNER_TYPED_TEST -DVMX_NESTED_OWNER_CONTEXT_TEST -DVMX_NESTED_OWNER_TYPED_CONTEXT_TEST ../krnl/tests/nested_owner_typed_contexts.c ../krnl/nested_owner_typed.c ../krnl/nested_ept.c ../krnl/nested_ept_contexts.c ../krnl/nested_ept_sparse.c ../krnl/nested_owner_pool.c ../krnl/domain_typed_lease.c -o "$out/contexts"
cc "${flags[@]}" -DVMX_NESTED_DOMAIN_TEST -DVMX_NESTED_OWNER_TYPED_TEST -DVMX_NESTED_OWNER_CONTEXT_TEST -DVMX_NESTED_OWNER_TYPED_CONTEXT_TEST ../krnl/tests/nested_owner_typed_bootstrap.c ../krnl/nested_owner_sparse.c ../krnl/nested_owner_typed.c ../krnl/nested_ept.c ../krnl/nested_ept_contexts.c ../krnl/nested_ept_sparse.c ../krnl/nested_owner_pool.c ../krnl/domain_typed_lease.c -o "$out/bootstrap"
cc "${flags[@]}" ../krnl/tests/domain_typed_lease.c ../krnl/domain_typed_lease.c -o "$out/history"
for t in aliases owner host gate setup contexts bootstrap history; do ASAN_OPTIONS=detect_leaks=0 "$out/$t"; done
make smp-nested-typed-contexts
for obj in build/k-nested38b-guest.o build/k-nested38b-adapter.o build/k-owner-typed-context.o build/k-owner-typed-context-sparse.o; do
 nm -u "$obj" > "$out/undefined"
 ! grep -E ' (pmm_alloc_page|pmm_free_page|log_text|log_hex|panic|vmx_guest_enter)$' "$out/undefined"
done
set +e
QEMU_CPUS=2 ESP_DIR=build/smp-nested-typed-contexts-esp timeout 90 scripts/run.sh smoke >build/nested38b-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'SMP.N20_BLOCKED CPUID_VMX0' build/nested38b-tcg.log
! grep -q 'SMP.N38B_PASS\|PANIC' build/nested38b-tcg.log
echo 'PASS N38B pure typed inventory/lease/owner/current audit and TCG skip; actual MP nested UC requires KVM'
