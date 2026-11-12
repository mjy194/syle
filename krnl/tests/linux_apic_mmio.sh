#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d /tmp/syle-lamm-test.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
api=${GUEST_APIC_API_DIR:-krnl}
decoder=${MMIO_DECODER_DIR:-krnl}
sources="krnl/linux_apic_mmio.c krnl/linux_apic_msr.c krnl/linux_mmio_memory.c $decoder/guest_mmio_mov.c $api/guest_apic.c $api/lapic.c $api/lapic_timer.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -I"$api" -I"$decoder" -fsanitize=address,undefined -g krnl/host-tests/linux_apic_mmio.c $sources -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -I"$api" -I"$decoder" -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/linux_apic_mmio.c -o "$tmp/model.o"
