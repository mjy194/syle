#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p tests/build
${CC:-cc} -I"${EFI_PREFIX:-/usr}/include/efi" -I"${EFI_PREFIX:-/usr}/include/efi/x86_64" \
    -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=undefined \
    tests/paging_alias.c -o tests/build/paging_alias
tests/build/paging_alias
