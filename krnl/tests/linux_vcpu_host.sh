#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
out="$tmp/test"
mkdir -p "$(dirname "$out")"
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined "$here/linux_vcpu_host.c" "$here/../linux_vcpu_host.c" "$here/../linux_vcpu_prepare.c" -o "$out"
ASAN_OPTIONS=detect_leaks=0 "$out"
cc -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -mgeneral-regs-only -mno-red-zone -c "$here/../linux_vcpu_host.c" -o "$out.o"
