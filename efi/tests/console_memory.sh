#!/usr/bin/env bash
# Host-only checks of framebuffer writes using guarded ordinary memory.
set -euo pipefail
cd "$(dirname "$0")/.."
EFI_PREFIX=${EFI_PREFIX:-/usr}
mkdir -p build
"${CC:-gcc}" \
  -I"$EFI_PREFIX/include/efi" -I"$EFI_PREFIX/include/efi/x86_64" \
  -ffunction-sections -fdata-sections -Wl,--gc-sections -fshort-wchar \
  -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  tests/console_memory.c -o build/console_memory_test
# No allocations in the module. Disable LSan because it cannot operate under
# ptrace in the cloud sandbox; ASan and UBSan remain active.
ASAN_OPTIONS="${ASAN_OPTIONS:+$ASAN_OPTIONS:}detect_leaks=0" \
  ./build/console_memory_test
