#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined krnl/host-tests/firmware_low_vector_deadline.c krnl/firmware_timer_stage.c krnl/firmware_timer_session.c krnl/lapic_timer.c krnl/linux_host_clock.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
