#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
base=$(CDPATH= cd -- "$here/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
for flags in "" "-DFFC_NO_KERNEL_BLOBS" "-DFFC_BOOT_MENU_DEFAULT" "-DFFC_BOOT_MENU_DEFAULT -DFFC_NO_KERNEL_BLOBS"; do
 cc $flags -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I"$here" -I"$base" \
 "$base/firmware_layout.c" "$base/firmware_memory.c" "$base/firmware_mmio_memory.c" "$base/firmware_fwcfg_target.c" "$base/firmware_fwcfg_transaction.c" "$base/firmware_fwcfg.c" "$base/firmware_piix_pm.c" "$base/firmware_cpuhp_absent.c" "$base/firmware_port92.c" "$base/firmware_pmtimer.c" "$base/firmware_pci_probe.c" "$here/firmware_fwcfg_bootmenu.c" -o "$tmp/test"
 ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
done
