#!/usr/bin/env bash
# Pure N38C-A: never invokes make, privileged VMX or writes efi/build.
set -euo pipefail
cd "$(dirname "$0")/../.."
efi/tests/nested38a.sh
cd krnl
out=$(mktemp -d /tmp/syle-nested38ca.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined)
cc "${flags[@]}" typed_root_lease_test.c typed_root_lease.c typed_directory.c -o "$out/lease"
cc "${flags[@]}" typed_root_registry_test.c typed_root_registry.c typed_root_lease.c typed_directory_registry.c typed_directory.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c nested_ept.c -o "$out/registry"
ASAN_OPTIONS=detect_leaks=0 "$out/lease"
ASAN_OPTIONS=detect_leaks=0 "$out/registry"
echo 'PASS N38C-A pure stable typed roots and actual registry lifecycle; no runtime or hardware integration'
