#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out=${1:?new output directory required};test ! -e "$out";mkdir -p "$out"
for mode in 0 1; do
 cc -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fpie -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -DNMI_PROBE_RUNNING=$mode -c tests/virtio_ro/nmi_probe_manager.c -o "$out/manager-$mode.o"
 cc -g -fno-pie -c tests/virtio_ro/nmi_probe_entry.S -o "$out/entry-$mode.o"
 ld -nostdlib -static -T user/mgmt/link.ld "$out/entry-$mode.o" "$out/manager-$mode.o" -o "$out/mgmt-nmi-$mode.elf"
 nm -n "$out/mgmt-nmi-$mode.elf" > "$out/mgmt-nmi-$mode.nm"
 test "$(stat -c %s "$out/mgmt-nmi-$mode.elf")" -lt 16384
 sha256sum "$out/mgmt-nmi-$mode.elf"
done
cc -std=c11 -Wall -Wextra -Werror tests/virtio_ro/nmi_probe_layout.c -o "$out/layout-tool"
"$out/layout-tool" > "$out/layout.txt"
