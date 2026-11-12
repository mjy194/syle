#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/smp_acpi.c ../krnl/smp_acpi.c -o build/smp-acpi-test
ASAN_OPTIONS=detect_leaks=0 build/smp-acpi-test
cc -g -Wall -Wextra -Werror -mgeneral-regs-only -no-pie -fsanitize=address,undefined ../krnl/tests/smp_bootstrap.c ../krnl/smp_cpu.c ../krnl/smp_entry.S -o build/smp-bootstrap-test
ASAN_OPTIONS=detect_leaks=0 build/smp-bootstrap-test
make smp
set +e
QEMU_CPUS=2 ESP_DIR=build/smp-esp timeout 90 scripts/run.sh smoke >build/smp-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'SMP.N18_PASS actual_AP=1 mailbox=3 parked=1' build/smp-tcg.log
! grep -q 'PANIC' build/smp-tcg.log
echo 'PASS N18 two actual emulated CPUs INIT/SIPI/mailbox; KVM separate'
