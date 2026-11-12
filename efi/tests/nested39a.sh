#!/usr/bin/env bash
# Pure software only: no make, build/ writes, VMX or other hardware execution.
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../.."
efi/tests/nested38ca.sh
out=$(mktemp -d /tmp/syle-nested39a.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Ikrnl)
sources=(krnl/typed_root_concurrent_test.c krnl/typed_root_lease.c krnl/typed_directory_registry.c krnl/typed_directory.c krnl/nested_owner_pool.c krnl/nested_ept_contexts.c krnl/nested_ept_sparse.c krnl/nested_ept.c)
cc "${flags[@]}" "${sources[@]}" krnl/typed_root_registry.c -o "$out/concurrent"
ASAN_OPTIONS=detect_leaks=0 "$out/concurrent"
# Demonstrate this test rejects reintroducing either old global assumption.
python3 - "$out" <<'PY'
from pathlib import Path
import sys
source=Path('krnl/typed_root_registry.c').read_text()
changes={
 'global_fence':('if(!supplied||!healthy(b)||', 'if(directory->version!=version||!supplied||!healthy(b)||'),
 'adjacent_commit':('(b->root_scoped?s->receipt_version>s->evidence_version:', '(b->root_scoped?s->receipt_version==s->evidence_version+1:'),
}
for name,(old,new) in changes.items():
 assert source.count(old)==1,(name,source.count(old))
 Path(sys.argv[1],name+'.c').write_text(source.replace(old,new))
PY
for mutant in global_fence adjacent_commit; do
 cc "${flags[@]}" "${sources[@]}" "$out/$mutant.c" -o "$out/$mutant"
 set +e
 ASAN_OPTIONS=detect_leaks=0 "$out/$mutant" --interleaving-only >"$out/$mutant.log" 2>&1
 status=$?
 set -e
 [[ $status == 134 ]]
 grep -q "concurrent_success.*Assertion.*nested_ept_contexts_destroy" "$out/$mutant.log"
 echo "PASS N39A negative witness rejects restored $mutant"
done
echo 'PASS N39A pure concurrent owner-local retirement; runtime remains legacy serial, no hardware claim'
