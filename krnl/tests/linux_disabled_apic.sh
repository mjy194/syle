#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g krnl/host-tests/linux_disabled_apic.c krnl/linux_disabled_apic.c -o "$t/test"
ASAN_OPTIONS=detect_leaks=0 "$t/test"
cc -std=c11 -Wall -Wextra -Werror -ffreestanding -mgeneral-regs-only -mno-red-zone -c krnl/linux_disabled_apic.c -o "$t/native.o"
