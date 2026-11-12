#!/usr/bin/env bash
# Host production bridge checks only; no build/ESP changes or VMX claims.
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../.."
out=$(mktemp -d /tmp/syle-nested41b.XXXXXX)
trap 'rm -rf "$out"' EXIT
sources=(krnl/typed_root_atomic_test.c krnl/typed_root_atomic.c krnl/typed_root_registry.c krnl/typed_root_local_provider.c krnl/typed_root_lease.c krnl/typed_directory_registry.c krnl/typed_directory.c krnl/nested_owner_pool.c krnl/nested_ept_contexts.c krnl/nested_ept_sparse.c krnl/nested_ept.c krnl/nested_atomic_catalog.c krnl/nested_root_catalog.c krnl/nested_ept_atomic.c krnl/ad_updater_gate.c)
cc -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -Ikrnl "${sources[@]}" -o "$out/bridge"
ASAN_OPTIONS=detect_leaks=0 "$out/bridge"
python3 efi/tests/nested41b_independent_review.py
echo 'PASS N41B production atomic lifecycle/refill bridge; no VMX integration'
