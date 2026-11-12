#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
"${CC:-gcc}" -I"${EFI_PREFIX:-/usr}/include/efi" -I"${EFI_PREFIX:-/usr}/include/efi/x86_64" \
  -DSYLE_KERNEL -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -Wall -Wextra -Werror -fsanitize=address,undefined -g tests/ms01_uart.c -o build/ms01_uart
ASAN_OPTIONS=detect_leaks=0 ./build/ms01_uart
