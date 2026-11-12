#!/usr/bin/env bash
# Production bounded overlap protocol, actual two-thread owner-private bridge
# and pools, actual runtime gate/completion slices, then optional build/TCG.
# Physical INVEPT/VMWRITE are host mocks: exact-ESP isolated KVM is separate.
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../.."
out=$(mktemp -d /tmp/syle-nested39b2.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Ikrnl)
common=(krnl/typed_root_local_provider.c krnl/typed_root_lease.c krnl/typed_directory_registry.c krnl/typed_directory.c krnl/nested_owner_pool.c krnl/nested_ept_contexts.c krnl/nested_ept_sparse.c krnl/nested_ept.c)
cc "${flags[@]}" krnl/typed_root_overlap_test.c krnl/typed_root_overlap.c -o "$out/protocol"
ASAN_OPTIONS=detect_leaks=0 "$out/protocol"
cc "${flags[@]}" -pthread krnl/typed_root_overlap_concurrent_test.c krnl/typed_root_overlap.c krnl/typed_root_registry.c "${common[@]}" -o "$out/concurrent"
ASAN_OPTIONS=detect_leaks=0 "$out/concurrent"
cc "${flags[@]}" krnl/typed_root_local_provider_test.c krnl/typed_root_registry.c "${common[@]}" -o "$out/provider"
ASAN_OPTIONS=detect_leaks=0 "$out/provider"
python3 - "$out" <<'PY'
from pathlib import Path
import sys
s=Path('krnl/typed_root_registry.c').read_text()
old='if(!supplied||!healthy(b)||'
assert s.count(old)==1
Path(sys.argv[1],'global_fence.c').write_text(s.replace(old,'if(directory->version!=version||!supplied||!healthy(b)||'))
PY
cc "${flags[@]}" -pthread krnl/typed_root_overlap_concurrent_test.c krnl/typed_root_overlap.c "$out/global_fence.c" "${common[@]}" -o "$out/global_fence"
set +e
ASAN_OPTIONS=detect_leaks=0 "$out/global_fence" >"$out/global_fence.log" 2>&1
status=$?
set -e
[[ $status == 134 ]]
grep -q 'select_use.*Assertion.*nested_ept_contexts_select' "$out/global_fence.log"
echo 'PASS N39B2 actual two-thread success rejects restored global-directory fence'
python3 efi/tests/typed_overlap_runtime_gates.py
python3 efi/tests/nested_owner_completion.py
[[ ${N39B2_PURE_ONLY:-0} != 1 ]] || exit 0
cd efi
make smp-nested-typed-overlap
for obj in build/k-nested39b2-guest.o build/k-nested39b2-adapter.o build/k-owner-overlap-typed.o build/k-owner-overlap-sparse.o build/k-typed-root-local-provider.o build/k-typed-root-overlap.o; do
 nm -u "$obj" > "$out/undefined"
 ! grep -E ' (pmm_alloc_page|pmm_free_page|log_text|log_hex|panic|vmx_guest_enter)$' "$out/undefined"
done
set +e
QEMU_CPUS=2 ESP_DIR=build/smp-nested-typed-overlap-esp timeout 90 scripts/run.sh smoke >build/nested39b2-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'SMP.N20_BLOCKED CPUID_VMX0' build/nested39b2-tcg.log
! grep -q 'SMP.N39B2_PASS\|SMP.N39B1_PASS\|SMP.N38CB_PASS\|PANIC' build/nested39b2-tcg.log
echo 'PASS N39B2 overlap provider/protocol/gates/completion/compile and TCG capability skip; actual bound hardware overlap requires KVM'
