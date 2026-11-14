#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT HUP INT TERM
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined tests/virtio_ro/vmx_close_barrier_test.c krnl/linux_vcpu_host.c krnl/linux_vcpu_prepare.c -o "$t/test"
ASAN_OPTIONS=detect_leaks=0 "$t/test"
