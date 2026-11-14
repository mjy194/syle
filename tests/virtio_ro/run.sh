#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -pedantic -ffreestanding -c backend.c -o "$tmp/backend.o"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -g backend.c test.c -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
cc -std=c11 -Wall -Wextra -Werror -pedantic -ffreestanding -c transport.c -o "$tmp/transport.o"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -g backend.c transport.c transport_test.c -o "$tmp/transport_test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/transport_test"
cc -std=c11 -Wall -Wextra -Werror -pedantic -ffreestanding -c platform.c -o "$tmp/platform.o"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -g backend.c transport.c platform.c platform_test.c -o "$tmp/platform_test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/platform_test"
cc -std=c11 -Wall -Wextra -Werror -pedantic -ffreestanding -c lgm_audit.c -o "$tmp/lgm_audit.o"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -g lgm_audit.c lgm_audit_test.c -o "$tmp/lgm_audit_test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/lgm_audit_test"
cc -std=c11 -Wall -Wextra -Werror -pedantic -ffreestanding -c lease_model.c -o "$tmp/lease_model.o"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -g lease_model.c lease_model_test.c -o "$tmp/lease_model_test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/lease_model_test"
cc -DPMM_TEST_PHYS_BITS=36 -Dpmm_free_page=pmm_raw_free_page -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g -c ../../krnl/memory.c -o "$tmp/pmm_raw.o"
cc -std=c11 -Wall -Wextra -Werror -pedantic -ffreestanding -c pmm_authority.c -o "$tmp/pmm_authority.o"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g lease_model.c pmm_authority.c pmm_authority_test.c "$tmp/pmm_raw.o" -o "$tmp/pmm_authority_test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/pmm_authority_test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/pmm_authority_test" fragmentation
# Existing PMM clients retain their unowned allocation/free behavior under the
# optional wrapper link. These exercise the actual existing test source.
cc -DPMM_TEST_PHYS_BITS=36 -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g lease_model.c pmm_authority.c ../../krnl/tests/memory.c "$tmp/pmm_raw.o" -o "$tmp/pmm_compat"
ASAN_OPTIONS=detect_leaks=0 "$tmp/pmm_compat"
ASAN_OPTIONS=detect_leaks=0 "$tmp/pmm_compat" fragmentation
ASAN_OPTIONS=detect_leaks=0 "$tmp/pmm_compat" zero
ASAN_OPTIONS=detect_leaks=0 "$tmp/pmm_compat" single
