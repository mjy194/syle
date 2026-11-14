#!/bin/sh
set -eu
package=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
root=$(CDPATH= cd -- "$1" && pwd)
for ext in c h; do
 cmp "$package/compact-host/firmware_observer_compact.$ext" "$package/overlay/krnl/firmware_observer_compact.$ext"
 cmp "$package/compact-host/firmware_observer_compact.$ext" "$root/krnl/firmware_observer_compact.$ext"
done
(cd "$package/compact-host" && sha256sum -c frozen.sha256)
test ! -e "$root/tests/nodma_overlay_host"
mkdir -p "$root/tests/nodma_overlay_host"
cp "$package"/host-tests/* "$root/tests/nodma_overlay_host/"
cd "$root/tests/nodma_overlay_host"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
common='backend.c transport.c firmware_pci_bus.c pci_config_projection.c pci_probe_nodma.c ../../krnl/firmware_pci_probe.c ../../krnl/firmware_piix_pm.c ../../krnl/firmware_pmtimer.c ../../krnl/firmware_cpuhp_absent.c ../../krnl/firmware_port92.c'
for test in pci_config_projection_test pci_enumeration_test pci_probe_nodma_test firmware_pci_bus_test; do
 cc -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -I. "$test.c" $common -o "$tmp/$test"
 ASAN_OPTIONS=detect_leaks=0 "$tmp/$test"
done
for profile in vfb_init vfb_init_projected; do
 cc -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -I. -DBUS_INIT=$profile firmware_bus_disk_test.c firmware_ram_adapter.c ../../krnl/firmware_memory.c ../../krnl/firmware_layout.c $common -o "$tmp/disk"
 ASAN_OPTIONS=detect_leaks=0 "$tmp/disk"
done
python3 pci_observer_shadow.py "$root/krnl/firmware_ovmf_quiet_observe.c" "$tmp/shadow.c"
cc -std=c11 -Wall -Wextra -Werror -Wno-misleading-indentation -pedantic -O1 -g -fsanitize=address,undefined -I. "$tmp/shadow.c" $common -o "$tmp/shadow"
ASAN_OPTIONS=detect_leaks=0 "$tmp/shadow"
