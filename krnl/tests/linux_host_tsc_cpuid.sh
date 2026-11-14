#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
b=$(mktemp -d);trap 'rm -rf "$b"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined linux_host_tsc_cpuid.c linux_vcpu_policy.c tests/linux_host_tsc_cpuid.c -o "$b/test"
ASAN_OPTIONS=detect_leaks=0 "$b/test"

EXTRA_CLOCK_DEFS=-DMG_HOST_CLOCK_TSC_FREQ EXTRA_CLOCK_SOURCES=krnl/linux_host_tsc_cpuid.c sh tests/linux_host_clock_dispatch.sh
