#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined pci_config_projection.c pci_config_projection_test.c ../../krnl/firmware_pmtimer.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
