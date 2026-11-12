#!/bin/sh
# Bounded read-only diagnostic host tests; no VM or PF policy changes.
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Ikrnl krnl/linux_pf_diagnostic.c krnl/linux_pt_diag.c krnl/host-tests/linux_cpl3_pf_diagnostic.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -Ikrnl -c krnl/linux_pf_diagnostic.c -o "$tmp/diag.o"
