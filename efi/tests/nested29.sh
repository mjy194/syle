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
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept_ad0.c ../krnl/nested_ept.c -o build/nested-ept-ad0-test
ASAN_OPTIONS=detect_leaks=0 build/nested-ept-ad0-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_memory_type.c ../krnl/nested_memory_type.c -o build/nested-memory-type-test
ASAN_OPTIONS=detect_leaks=0 build/nested-memory-type-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/nested_ept_sparse_typed.c ../krnl/nested_ept_sparse.c ../krnl/nested_memory_type.c -o build/nested-sparse-typed-test
ASAN_OPTIONS=detect_leaks=0 build/nested-sparse-typed-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/cache_page.c ../krnl/cache_page.c -o build/nested-uc-lease-test
ASAN_OPTIONS=detect_leaks=0 build/nested-uc-lease-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/cache_lease_refs.c ../krnl/cache_lease_refs.c -o build/cache-lease-refs-test
ASAN_OPTIONS=detect_leaks=0 build/cache-lease-refs-test
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined ../krnl/tests/cache_lease_contexts.c ../krnl/cache_lease_refs.c ../krnl/nested_ept_contexts.c ../krnl/nested_ept_sparse.c -o build/cache-lease-contexts-test
ASAN_OPTIONS=detect_leaks=0 build/cache-lease-contexts-test
make vmx-nested-uc-pat
set +e
ESP_DIR=build/vmx-nested-uc-pat-esp timeout 90 scripts/run.sh smoke >build/nested29-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'NESTED.UC_PLATFORM_BLOCKED no_transition' build/nested29-tcg.log
! grep -q 'NESTED.N29_PASS\|VMX.ROOT_ENTERED\|PANIC' build/nested29-tcg.log
echo 'PASS N29 pure policies/build/TCG skip; nested EPT hardware not tested'
