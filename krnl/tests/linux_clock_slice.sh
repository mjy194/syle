#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
b=$(mktemp -d); trap 'rm -rf "$b"' EXIT HUP INT TERM
for bits in 28 24 20; do
 ${CC:-cc} -std=c11 -Wall -Wextra -Werror -DLGS_PREEMPT_BITS=$bits linux_guest_session.c tests/linux_guest_session.c -o "$b/test"
 "$b/test"
done
for bits in 15 29; do
 if ${CC:-cc} -std=c11 -DLGS_PREEMPT_BITS=$bits -c linux_guest_session.c -o "$b/bad.o" 2>/dev/null; then
  echo "invalid slice accepted" >&2; exit 1
 fi
done
echo 'clock slice default/A-B + invalid bounds PASS'
${CC:-cc} -std=c11 -Wall -Wextra -Werror tests/linux_clock_diag.c -o "$b/diag"
"$b/diag"
echo 'clock aggregate diagnostic PASS'
