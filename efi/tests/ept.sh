#!/usr/bin/env bash
# Unit/build + TCG unavailable branch only; no EPT hardware claim.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -DEPT_HOST_TEST -I"${EFI_INC:-${EFI_PREFIX:-/usr}/include/efi}" -I"${EFI_INC:-${EFI_PREFIX:-/usr}/include/efi}/x86_64" -fsanitize=address,undefined ../krnl/tests/ept.c ../krnl/ept.c ../krnl/ept_payload.S -o build/ept-policy-test
ASAN_OPTIONS=detect_leaks=0 build/ept-policy-test
make vmx-ept
set +e
ESP_DIR=build/vmx-ept-esp timeout 90 scripts/run.sh smoke > build/ept-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/ept-tcg.log
! grep -q 'EPT.DIAGNOSTIC_PASS\|VMX.ROOT_ENTERED\|PANIC' build/ept-tcg.log
echo 'PASS EPT unit/compile/TCG skip; hardware EPT untested'
