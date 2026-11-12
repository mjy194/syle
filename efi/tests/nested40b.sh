#!/usr/bin/env bash
# Exact production opt-in adapter plus registry/pool. Hardware is separately proved.
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../.."
out=$(mktemp -d /tmp/syle-nested40b.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined)
read -r -a profile <<< "$(make -s -C efi -f Makefile -f - print-n40 <<'MAKE'
print-n40:
	@echo $(N40BFLAGS)
MAKE
)"
cc "${flags[@]}" "${profile[@]}" krnl/tests/nested_owner_catalog.c krnl/nested_owner_dynamic_typed.c krnl/nested_owner_sparse.c krnl/nested_root_catalog.c krnl/typed_root_registry.c krnl/typed_root_lease.c krnl/typed_directory_registry.c krnl/typed_directory.c krnl/nested_ept.c krnl/nested_ept_contexts.c krnl/nested_ept_sparse.c krnl/nested_owner_pool.c -o "$out/owner"
ASAN_OPTIONS=detect_leaks=0 "$out/owner"
for test in nested_catalog_profile nested_domain_profile nested_profile; do
 cc "${flags[@]}" "krnl/tests/$test.c" krnl/nested_profile.c krnl/nested_vmx.c -o "$out/$test"
 ASAN_OPTIONS=detect_leaks=0 "$out/$test"
done
python3 efi/tests/nested_owner_completion.py
python3 efi/tests/nested40b_independent_review.py
[[ ${N40B_PURE_ONLY:-0} != 1 ]] || exit 0
cd efi
make smp-nested-root-catalog
for obj in build/k-nested40b-guest.o build/k-nested40b-adapter.o build/k-owner-catalog-typed.o build/k-owner-catalog-sparse.o build/k-nested-root-catalog.o; do
 nm -u "$obj" > "$out/undefined"
 ! grep -E ' (pmm_alloc_page|pmm_free_page|log_text|log_hex|panic|vmx_guest_enter)$' "$out/undefined"
done
set +e
QEMU_CPUS=2 ESP_DIR=build/smp-nested-root-catalog-esp timeout 90 scripts/run.sh smoke >build/nested40b-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'SMP.N20_BLOCKED CPUID_VMX0' build/nested40b-tcg.log
! grep -q 'SMP.N40B_PASS\|SMP.N39B2_PASS\|PANIC' build/nested40b-tcg.log
echo 'PASS N40B production catalog adapter/profile/completion/compile and TCG capability skip; exact-ESP KVM remains separate'
