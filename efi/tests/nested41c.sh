#!/usr/bin/env bash
# Host production checks plus a real image build and honest TCG capability skip.
# Actual VMX, physical INVEPT and the eight checkpoints require separate hardware.
set -euo pipefail
ulimit -c 0
cd "$(dirname "$0")/../.."
mode=${1:-all}
case "$mode" in all|host|runtime) ;; *) echo 'Usage: nested41c.sh [all|host|runtime]' >&2; exit 2;; esac
out=$(mktemp -d /tmp/syle-nested41c.XXXXXX)
trap 'rm -rf "$out"' EXIT
if [[ $mode != runtime ]]; then
 efi/tests/nested41c_memory.sh
 flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined)
 for test in nested_atomic_profile nested_domain_profile nested_catalog_profile nested_profile; do
  cc "${flags[@]}" "krnl/tests/$test.c" krnl/nested_profile.c krnl/nested_vmx.c -o "$out/$test"
  ASAN_OPTIONS=detect_leaks=0 "$out/$test"
 done
 python3 efi/tests/nested41c_integration.py
 python3 efi/tests/nested41c_independent_review.py
 echo 'PASS N41C host production memory/profile/entry/dispatch/adapter/integration and weakening checks; no VMX claim'
fi
if [[ $mode != host ]]; then
 make -C efi smp-nested-atomic-catalog
 for obj in k-nested41c-owner.o k-nested41c-sparse.o k-nested41c-vcpu.o k-nested41c-runtime.o k-typed-root-atomic.o; do
  nm -u "efi/build/$obj" > "$out/undefined"
  ! grep -E ' (pmm_alloc_page|pmm_free_page|log_text|log_hex|panic|vmx_guest_enter)$' "$out/undefined"
 done
 nm -u efi/build/k-nested41c-adapter.o > "$out/adapter-undefined"
 for symbol in nested_owner_atomic_entry nested_owner_atomic_fault nested_domain_dispatch_physical_suspend_ept nested_domain_dispatch_physical_resume nested_vcpu_authorize nested_vcpu_enter; do
  grep -Eq " U $symbol$" "$out/adapter-undefined"
 done
 set +e
 QEMU_CPUS=2 ESP_DIR=build/smp-nested-atomic-catalog-esp timeout 90 efi/scripts/run.sh smoke >efi/build/nested41c-tcg.log 2>&1
 status=$?
 set -e
 [[ $status == 33 ]]
 grep -Fq 'SMP.N20_BLOCKED CPUID_VMX0' efi/build/nested41c-tcg.log
 ! grep -Eq 'SMP.N41C_PASS|N41C_CHECKPOINT=|PANIC' efi/build/nested41c-tcg.log
 echo 'PASS N41C image/production symbols and TCG CPUID_VMX0 capability skip; exact-image VMX/INVEPT acceptance remains separate'
fi
if [[ $mode == all ]]; then
 echo 'PASS N41C production atomic catalog runtime host checks/build/TCG skip; hardware and full regression recorded separately'
fi
