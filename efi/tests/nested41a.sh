#!/usr/bin/env bash
# Pure N41A; no build/ESP changes, VMX, hardware invalidation or retirement claim.
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../.."
out=$(mktemp -d /tmp/syle-nested41a.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -Ikrnl)
cc "${flags[@]}" krnl/nested_atomic_catalog_test.c krnl/nested_atomic_catalog.c krnl/nested_root_catalog.c krnl/nested_ept_atomic.c krnl/ad_updater_gate.c krnl/nested_ept.c krnl/nested_ept_sparse.c -o "$out/catalog"
ASAN_OPTIONS=detect_leaks=0 "$out/catalog"
python3 efi/tests/nested41a_independent_review.py
echo 'PASS N41A atomic catalog/whole-operation substrate; no VMX runtime integration'
