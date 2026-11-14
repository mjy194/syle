#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined pci_probe_nodma.c pci_probe_nodma_test.c pci_config_projection.c firmware_pci_bus.c backend.c transport.c ../../krnl/firmware_pci_probe.c ../../krnl/firmware_piix_pm.c ../../krnl/firmware_pmtimer.c ../../krnl/firmware_cpuhp_absent.c ../../krnl/firmware_port92.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
cc -std=c11 -O1 -ffreestanding -fno-builtin -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-pic -fno-pie -c pci_probe_nodma.c -o "$tmp/probe.o"
objdump -dr "$tmp/probe.o" > "$tmp/probe.asm"
echo 'no-DMA probe kernel-ABI object compiled (not linked/booted)'
