#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p tests/build
${CC:-cc} -I"${EFI_PREFIX:-/usr}/include/efi" -I"${EFI_PREFIX:-/usr}/include/efi/x86_64" \
    -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=undefined \
    tests/paging_rw_wb.c -o tests/build/paging_rw_wb
UBSAN_OPTIONS=halt_on_error=1 tests/build/paging_rw_wb
