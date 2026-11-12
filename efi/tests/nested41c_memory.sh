#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"${EFI_INC:-${EFI_PREFIX:-/usr}/include/efi}" -I"${EFI_INC:-${EFI_PREFIX:-/usr}/include/efi}/x86_64" \
 -DVMX_NESTED_DOMAIN_TEST efi/tests/nested41c_memory.c krnl/nested_atomic_memory.c \
 krnl/nested_domain_memory.c krnl/nested_root_catalog.c krnl/nested_ept.c -o "$tmp/check"
ASAN_OPTIONS=detect_leaks=0 "$tmp/check"
