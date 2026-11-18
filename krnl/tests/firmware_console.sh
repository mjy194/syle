#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I"$here/.." "$here/firmware_console.c" "$here/../firmware_console.c" "$here/../linux_uart.c" "$here/../firmware_piix_pm.c" "$here/../firmware_pci_probe.c" "$here/../firmware_pmtimer.c" "$here/../firmware_cpuhp_absent.c" "$here/../firmware_port92.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
