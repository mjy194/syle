#!/usr/bin/env bash
# Production EFI adapter host mocks only; ELF argument is parsed, never executed.
set -euo pipefail
cd "$(dirname "$0")/.."
EFI_PREFIX=${EFI_PREFIX:-/usr}
fixture=${1:?Provide a kernel ELF fixture (read-only)}
mkdir -p build
"${CC:-gcc}" -I"$EFI_PREFIX/include/efi" -I"$EFI_PREFIX/include/efi/x86_64" \
  -ffunction-sections -fdata-sections -Wl,--gc-sections -fshort-wchar \
  -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  tests/firmware_diagnostics.c ../boot/kernel_elf.c -o build/firmware_diagnostics_test
ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0" \
  UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1}" \
  ./build/firmware_diagnostics_test "$fixture"
