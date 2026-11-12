#!/usr/bin/env bash
# N42A source-only production maintenance protocol. No VMX/ESP claim.
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../.."
out=$(mktemp -d /tmp/syle-nested42a.XXXXXX)
trap 'rm -rf "$out"' EXIT
python3 efi/tests/nested42a_freestanding.py
sources=(krnl/typed_root_atomic_maintenance_test.c krnl/typed_root_atomic.c krnl/typed_root_registry.c krnl/typed_root_local_provider.c krnl/typed_root_lease.c krnl/typed_directory_registry.c krnl/typed_directory.c krnl/nested_owner_pool.c krnl/nested_ept_contexts.c krnl/nested_ept_sparse.c krnl/nested_ept.c krnl/nested_atomic_catalog.c krnl/nested_root_catalog.c krnl/nested_ept_atomic.c krnl/ad_updater_gate.c)
cc -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined -Ikrnl "${sources[@]}" -o "$out/maintenance"
ASAN_OPTIONS=detect_leaks=0 "$out/maintenance"
python3 efi/tests/nested42a_independent_review.py
echo 'PASS N42A production AD-only maintenance; physical hooks are mocks'
