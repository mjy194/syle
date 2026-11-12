#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/ioapic.c ../krnl/ioapic.c ../krnl/device_bus.c -o build/ioapic-policy-test
ASAN_OPTIONS=detect_leaks=0 build/ioapic-policy-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/mmio_decode.c ../krnl/mmio_decode.c -o build/mmio-decode-test
ASAN_OPTIONS=detect_leaks=0 build/mmio-decode-test
make vmx-ioapic
set +e
ESP_DIR=build/vmx-ioapic-esp timeout 90 scripts/run.sh smoke > build/ioapic-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/ioapic-tcg.log
! grep -q 'DEVICE.IOAPIC_PASS\|VMX.ROOT_ENTERED\|PANIC' build/ioapic-tcg.log
echo 'PASS MMIO/IOAPIC unit/compile/TCG skip; hardware not tested'
