#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_operand.c ../krnl/nested_operand.c -o build/nested-operand-test
ASAN_OPTIONS=detect_leaks=0 build/nested-operand-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_profile.c ../krnl/nested_profile.c ../krnl/nested_vmx.c -o build/nested-profile-test
ASAN_OPTIONS=detect_leaks=0 build/nested-profile-test
./tests/nested0.sh
