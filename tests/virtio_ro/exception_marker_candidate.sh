#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT HUP INT TERM
python3 tests/virtio_ro/exception_marker_candidate.py "$t"
for unit in exceptions linux_exceptions; do
 defs='';test "$unit" != linux_exceptions || defs='-DLINUX_GUEST_PROBE'
 cc -fno-pie $defs -c "$t/$unit.S" -o "$t/$unit.o"
 # Private nmi byte stays local in the real candidate; expose only test object.
 test "$(nm -g --defined-only "$t/$unit.o" | grep -c ' nmi_entered$' || true)" = 0
 objcopy --globalize-symbol=nmi_entered --globalize-symbol=exception_entered "$t/$unit.o" "$t/$unit-test.o"
 cc -std=c11 -Wall -Wextra -Werror -no-pie -fsanitize=address,undefined tests/virtio_ro/exception_marker_test.c "$t/$unit-test.o" -o "$t/test"
 ASAN_OPTIONS=detect_leaks=0 "$t/test"
 objdump -dr --disassemble=exceptions_fatal_context "$t/$unit.o"
done
