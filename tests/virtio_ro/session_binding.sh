#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
b=$(mktemp -d); trap 'rm -rf "$b"' EXIT HUP INT TERM
${CC:-cc} -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer krnl/linux_guest_session.c krnl/linux_guest_role_binding.c tests/virtio_ro/session_binding_test.c -o "$b/test"
ASAN_OPTIONS=detect_leaks=0 "$b/test"
