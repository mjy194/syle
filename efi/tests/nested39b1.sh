#!/usr/bin/env bash
# Production root-local provider host tests plus opt-in runtime compile/TCG skip.
# Real INVEPT evidence requires separate isolated KVM with the frozen kernel.
set -euo pipefail
cd "$(dirname "$0")/../.."
out=$(mktemp -d /tmp/syle-nested39b1.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Ikrnl)
cc "${flags[@]}" krnl/typed_root_local_provider_test.c krnl/typed_root_local_provider.c \
 krnl/typed_root_registry.c krnl/typed_root_lease.c krnl/typed_directory_registry.c \
 krnl/typed_directory.c krnl/nested_owner_pool.c krnl/nested_ept_contexts.c \
 krnl/nested_ept_sparse.c krnl/nested_ept.c -o "$out/provider"
ASAN_OPTIONS=detect_leaks=0 "$out/provider"
python3 efi/tests/nested_owner_completion.py
cd efi
make smp-nested-typed-root-scoped
for obj in build/k-nested39b1-guest.o build/k-nested39b1-adapter.o build/k-owner-root-scoped-typed.o build/k-owner-root-scoped-sparse.o build/k-typed-root-local-provider.o; do
 nm -u "$obj" > "$out/undefined"
 ! grep -E ' (pmm_alloc_page|pmm_free_page|log_text|log_hex|panic|vmx_guest_enter)$' "$out/undefined"
done
set +e
QEMU_CPUS=2 ESP_DIR=build/smp-nested-typed-root-scoped-esp timeout 90 scripts/run.sh smoke >build/nested39b1-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'SMP.N20_BLOCKED CPUID_VMX0' build/nested39b1-tcg.log
! grep -q 'SMP.N39B1_PASS\|SMP.N38CB_PASS\|PANIC' build/nested39b1-tcg.log
echo 'PASS N39B1 production provider/completion/compile and TCG capability skip; serial rounds retained, actual bound INVEPT requires KVM'
