#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer firmware_pci_bus.c pci_config_projection.c firmware_ram_adapter.c firmware_bus_disk_test.c backend.c transport.c ../../krnl/firmware_memory.c ../../krnl/firmware_layout.c ../../krnl/firmware_pci_probe.c ../../krnl/firmware_piix_pm.c ../../krnl/firmware_pmtimer.c ../../krnl/firmware_cpuhp_absent.c ../../krnl/firmware_port92.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"

cc -DBUS_INIT=vfb_init_projected -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer firmware_pci_bus.c pci_config_projection.c firmware_ram_adapter.c firmware_bus_disk_test.c backend.c transport.c ../../krnl/firmware_memory.c ../../krnl/firmware_layout.c ../../krnl/firmware_pci_probe.c ../../krnl/firmware_piix_pm.c ../../krnl/firmware_pmtimer.c ../../krnl/firmware_cpuhp_absent.c ../../krnl/firmware_port92.c -o "$tmp/projected"
ASAN_OPTIONS=detect_leaks=0 "$tmp/projected"
