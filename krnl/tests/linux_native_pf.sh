#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
flags='-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g'
${CC:-cc} $flags -DMG_DIAGNOSTIC_GUEST_PF -Dlvp_prepare=lvp_prepare_diagnostic_pf -c krnl/linux_vcpu_prepare.c -o "$tmp/native.o"
${CC:-cc} $flags krnl/tests/linux_native_pf.c krnl/linux_vcpu_prepare.c "$tmp/native.o" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -DMG_NATIVE_GUEST_PF -c krnl/linux_vcpu_prepare.c -o "$tmp/native-freestanding.o"
