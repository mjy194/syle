#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
for flags in "" "-DFPI_HOSTBRIDGE_NO_BARS"; do
 cc $flags -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I"$base" \
 "$base/firmware_pci_probe.c" "$base/firmware_piix_pm.c" "$base/firmware_pmtimer.c" "$base/firmware_cpuhp_absent.c" "$base/firmware_port92.c" "$base/tests/firmware_pci_no_bars.c" -o "$tmp/test"
 ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
done
