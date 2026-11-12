#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../krnl"
out=$(mktemp -d /tmp/syle-n35a.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -pthread -fsanitize=address,undefined)
cc "${flags[@]}" tests/ad_entry_merge.c ad_entry_merge.c -o "$out/merge"
cc "${flags[@]}" tests/ad_updater_gate.c ad_updater_gate.c -o "$out/gate"
cc "${flags[@]}" tests/nested_ept_atomic.c nested_ept_atomic.c -o "$out/walk"
cc "${flags[@]}" tests/nested_ept_atomic_lease.c nested_ept_atomic_lease.c nested_ept_atomic.c ad_updater_gate.c -o "$out/lease"
for t in merge gate walk lease; do ASAN_OPTIONS=detect_leaks=0 "$out/$t"; done
echo 'PASS N35A pure concurrent atomic A/D and lease only; no runtime/VMX integration'
