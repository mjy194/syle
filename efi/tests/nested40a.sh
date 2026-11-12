#!/usr/bin/env bash
# N40A production catalog + actual typed registry. Pure; no make/VMX/build edits.
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../.."
out=$(mktemp -d /tmp/syle-nested40a.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Ikrnl)
cc "${flags[@]}" krnl/nested_root_catalog_test.c krnl/nested_root_catalog.c krnl/nested_ept.c -o "$out/catalog"
ASAN_OPTIONS=detect_leaks=0 "$out/catalog"
cc "${flags[@]}" krnl/nested_root_catalog_registry_test.c krnl/nested_root_catalog.c krnl/typed_root_registry.c krnl/typed_root_lease.c krnl/typed_directory_registry.c krnl/typed_directory.c krnl/nested_owner_pool.c krnl/nested_ept_contexts.c krnl/nested_ept_sparse.c krnl/nested_ept.c -o "$out/registry"
ASAN_OPTIONS=detect_leaks=0 "$out/registry"
python3 efi/tests/nested40a_independent_review.py
echo 'PASS N40A immutable catalog and authoritative owned backing; no VMX runtime claim'
