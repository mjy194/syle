#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../krnl"
out=$(mktemp -d /tmp/syle-n36a.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined)
cc "${flags[@]}" tests/domain_typed_inventory.c domain_typed_inventory.c -o "$out/audit"
cc "${flags[@]}" tests/domain_typed_lease.c domain_typed_lease.c -o "$out/lease"
cc "${flags[@]}" tests/domain_typed_integration.c domain_typed_inventory.c domain_typed_lease.c -o "$out/integration"
for t in audit lease integration; do ASAN_OPTIONS=detect_leaks=0 "$out/$t"; done
echo 'PASS N36A pure inventory/MP typed lease only; no runtime, cache transition or VMX execution'
