#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make ring3
for n in 0 1 2 3 4; do
    set +e
    timeout 90 scripts/run.sh "ring3-$n" > "build/ring3-$n.log" 2>&1
    status=$?
    set -e
    [[ $status == 33 ]] || { cat "build/ring3-$n.log"; echo "FAIL case $n exit=$status"; exit 1; }
    python3 - "$n" <<'PY'
from pathlib import Path
import sys
n = int(sys.argv[1])
s = Path(f'build/ring3-{n}.log').read_text(errors='replace')
markers = ['STAGE0.EXIT_BOOT_SERVICES_BEGIN', 'SYLE.KRNL_ENTRY',
           'RING3.FLAT_IMAGE_LOADED', 'RING3.OWN_CR3_AND_STACK', 'RING3.PASS']
p = [s.find(m) for m in markers]
assert all(i >= 0 for i in p) and p == sorted(p), s
assert 'RING3.FAIL' not in s and 'PANIC' not in s, s
if n == 0:
    assert 'SYLE.MGMT: ring3 management process online' in s
    assert 'RING3.EXIT syscall_count=0x0000000000000004' in s
else:
    vector, error = {1:(14,5), 2:(14,7), 3:(14,21), 4:(13,0)}[n]
    assert f'RING3.USER_FAULT vector=0x{vector:016x}' in s
    assert f'RING3.USER_FAULT error=0x{error:016x}' in s
print(f'PASS ring3 case {n}')
PY
done
