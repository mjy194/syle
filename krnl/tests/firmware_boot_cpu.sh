#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -DSYLE_FIRMWARE_BOOT_LINUX "$here/firmware_boot_cpu.c" "$here/../firmware_boot_cpu.c" "$here/../firmware_cpu_mode.c" "$here/../firmware_active_cpu_tsc.c" "$here/../firmware_active_cpu_de.c" "$here/../firmware_active_cpu.c" "$here/../firmware_cpu_profile.c" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined "$here/linux_gs_commit.c" "$here/../linux_gs_commit.c" "$here/../linux_vcpu_msr.c" -o "$tmp/gs"
ASAN_OPTIONS=detect_leaks=0 "$tmp/gs"
