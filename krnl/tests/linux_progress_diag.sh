#!/bin/sh
# Host only; no VM execution.
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
${CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Ikrnl krnl/linux_progress_diag.c krnl/pit0.c krnl/host-tests/linux_progress_diag.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
