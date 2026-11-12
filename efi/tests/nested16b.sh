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
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept_sparse_ad.c ../krnl/nested_ept_sparse.c -o build/nested-ept-sparse-ad-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-sparse-ad-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept_ad.c ../krnl/nested_ept_ad.c ../krnl/nested_ept.c -o build/nested-ept-ad-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-ad-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept_ad_prefix.c ../krnl/nested_ept_ad.c ../krnl/nested_ept.c -o build/nested-ept-ad-prefix-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-ad-prefix-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept_contexts.c ../krnl/nested_ept_contexts.c ../krnl/nested_ept_sparse.c -o build/nested-ept-contexts-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-contexts-test
make vmx-nested-ept-ad-context
set +e
ESP_DIR=build/vmx-nested-ept-ad-context-esp timeout 90 scripts/run.sh smoke >build/nested16b-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'VMX.UNAVAILABLE CPUID.VMX=0' build/nested16b-tcg.log
! grep -q 'NESTED.N16B_PASS\|VMX.ROOT_ENTERED\|PANIC' build/nested16b-tcg.log
echo 'PASS N16B pure policies/build/TCG skip; nested EPT hardware not tested'
