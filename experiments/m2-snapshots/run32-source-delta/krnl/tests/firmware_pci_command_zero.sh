#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
base=$(CDPATH= cd -- "$here/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
for flags in "" "-DFPI_HOSTBRIDGE_NO_BARS" "-DFPI_HOSTBRIDGE_READONLY_CONFIG" "-DFPI_HOSTBRIDGE_READONLY_CONFIG -DFPI_HOSTBRIDGE_COMMAND_ZERO"; do
 cc $flags -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I"$here" -I"$base" \
 "$base/firmware_pci_probe.c" "$base/firmware_piix_pm.c" "$base/firmware_pmtimer.c" "$base/firmware_cpuhp_absent.c" "$base/firmware_port92.c" "$here/firmware_pci_command_zero.c" -o "$tmp/test"
 ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
done
if cc -DFPI_HOSTBRIDGE_COMMAND_ZERO -I"$here" -c "$base/firmware_pci_probe.c" -o "$tmp/bad.o" 2>"$tmp/error"; then exit 1; fi
grep -q FPI_HOSTBRIDGE_COMMAND_ZERO_requires_READONLY_CONFIG "$tmp/error"
