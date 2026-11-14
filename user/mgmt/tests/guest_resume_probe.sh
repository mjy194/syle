#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-pie -no-pie "$root/user/mgmt/tests/guest_resume_probe.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
