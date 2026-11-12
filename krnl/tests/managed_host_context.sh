#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
b=$(mktemp -d);trap 'rm -rf "$b"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/managed_host_context.c -o "$b/test"
ASAN_OPTIONS=detect_leaks=0 "$b/test"
