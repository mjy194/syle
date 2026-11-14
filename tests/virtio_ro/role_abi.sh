#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../.." && pwd)
: "${ROLE_ABI_BASE:=HEAD}"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir "$tmp/old"
for f in linux_guest_memory.h linux_vcpu_host.h linux_vcpu_prepare.h; do git -C "$repo" show "$ROLE_ABI_BASE:krnl/$f" > "$tmp/old/$f"; done
cc -std=c11 -Wall -Wextra -Werror -I"$tmp/old" -I"$repo/krnl" "$here/role_abi.c" -o "$tmp/old-test"
cc -std=c11 -Wall -Wextra -Werror -I"$repo/krnl" "$here/role_abi.c" -o "$tmp/new-test"
"$tmp/old-test" > "$tmp/old.txt"
"$tmp/new-test" > "$tmp/new.txt"
cmp "$tmp/old.txt" "$tmp/new.txt"
printf 'legacy signatures/layout baseline %s: PASS\n' "$(git -C "$repo" rev-parse "$ROLE_ABI_BASE")"
cat "$tmp/new.txt"
