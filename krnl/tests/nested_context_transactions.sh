#!/bin/sh
# Host-only production context/sparse transaction contract checks.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d /tmp/syle-context-transactions.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -O2 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-pie -no-pie \
    nested_context_transactions_test.c nested_ept_contexts.c nested_ept_sparse.c \
    -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$tmp/test"
