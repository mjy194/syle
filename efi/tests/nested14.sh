#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./tests/nested1-policy.sh
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept.c ../krnl/nested_ept.c -o build/nested-ept-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept_sparse.c ../krnl/nested_ept_sparse.c -o build/nested-ept-sparse-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-sparse-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept_large.c ../krnl/nested_ept.c -o build/nested-ept-large-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-large-test
make vmx-nested-ept-large
set +e
ESP_DIR=build/vmx-nested-ept-large-esp timeout 90 scripts/run.sh smoke >build/nested14-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/nested14-tcg.log
! grep -q 'NESTED.N14_PASS\|VMX.ROOT_ENTERED\|PANIC' build/nested14-tcg.log
echo 'PASS N14 pure policies/build/TCG skip; nested EPT hardware not tested'
