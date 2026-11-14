#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined "$here/../firmware_pci_probe.c" "$here/firmware_pci_probe.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
