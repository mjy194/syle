#!/usr/bin/env bash
# Pure N38A only: deliberately never invokes make or writes efi/build.
set -euo pipefail
cd "$(dirname "$0")/../../krnl"
tmp=$(mktemp -d /tmp/syle-nested38a.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined)
run() { local name=$1; shift; cc "${flags[@]}" "$@" -o "$tmp/$name"; ASAN_OPTIONS=detect_leaks=0 "$tmp/$name"; }
run directory typed_directory_test.c typed_directory.c
run bridge typed_directory_registry_test.c typed_directory_registry.c typed_directory.c nested_ept_contexts.c nested_ept_sparse.c
run transactions nested_context_transactions_test.c nested_ept_contexts.c nested_ept_sparse.c
run assembled tests/typed_directory_contexts.c typed_directory.c nested_ept_contexts.c nested_ept_sparse.c
run contexts tests/nested_ept_contexts.c nested_ept_contexts.c nested_ept_sparse.c
run owner-contexts tests/nested_owner_contexts.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c
run lease-contexts tests/cache_lease_contexts.c cache_lease_refs.c nested_ept_contexts.c nested_ept_sparse.c
run sparse tests/nested_ept_sparse.c nested_ept_sparse.c
echo 'PASS N38A pure dynamic directory/registry lifecycle; no runtime integration or hardware validation'
