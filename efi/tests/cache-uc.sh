#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/cache_page.c ../krnl/cache_page.c -o build/cache-page-test
ASAN_OPTIONS=detect_leaks=0 build/cache-page-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/smp_acpi.c ../krnl/smp_acpi.c -o build/cache-acpi-test
ASAN_OPTIONS=detect_leaks=0 build/cache-acpi-test
make cache-uc
set +e
QEMU_CPUS=1 ESP_DIR=build/cache-uc-esp timeout 90 scripts/run.sh smoke >build/cache-uc-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'CACHE.N26_BLOCKED CPUID_VMX0' build/cache-uc-tcg.log
! grep -q 'CACHE.N26_PASS\|PANIC' build/cache-uc-tcg.log
echo 'PASS N26 pure alias/transition policy/build/TCG block; UC guest requires isolated single-CPU KVM'
