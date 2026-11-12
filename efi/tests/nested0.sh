#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_vmx.c ../krnl/nested_vmx.c -o build/nested0-test
ASAN_OPTIONS=detect_leaks=0 build/nested0-test
