#!/usr/bin/env bash
# Unit/build + TCG unavailable branch only; no EPT hardware claim.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -g -Wall -Wextra -Werror -DEPT_HOST_TEST -DVMX_STACKPART_TEST -I"${EFI_INC:-${EFI_PREFIX:-/usr}/include/efi}" -I"${EFI_INC:-${EFI_PREFIX:-/usr}/include/efi}/x86_64" -fsanitize=address,undefined ../krnl/tests/stackpart.c ../krnl/ept.c ../krnl/ept_payload.S -o build/stackpart-policy-test
ASAN_OPTIONS=detect_leaks=0 build/stackpart-policy-test
make vmx-stackpart
set +e
ESP_DIR=build/vmx-stackpart-esp timeout 90 scripts/run.sh smoke > build/stackpart-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/stackpart-tcg.log
! grep -q 'VCPU.STACKPART_DIAGNOSTIC_PASS\|VMX.ROOT_ENTERED\|PANIC' build/stackpart-tcg.log
echo 'PASS EPT unit/compile/TCG skip; hardware EPT untested'
