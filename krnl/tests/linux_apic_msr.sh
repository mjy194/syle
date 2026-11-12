#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d /tmp/syle-lams-test.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
api=${GUEST_APIC_API_DIR:-krnl}
sources="krnl/linux_apic_msr.c $api/guest_apic.c $api/lapic.c $api/lapic_timer.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -I"$api" -fsanitize=address,undefined -g krnl/host-tests/linux_apic_msr.c $sources -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -I"$api" -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/linux_apic_msr.c -o "$tmp/model.o"
