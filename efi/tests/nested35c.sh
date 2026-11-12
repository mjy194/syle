#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
san=(-g -Wall -Wextra -Werror -fsanitize=address,undefined)
cc "${san[@]}" ../krnl/tests/nested_domain.c ../krnl/nested_domain.c -o build/nested-domain-test
cc "${san[@]}" ../krnl/tests/nested_domain_dispatch.c ../krnl/nested_domain_dispatch.c ../krnl/nested_domain.c ../krnl/nested_vmx.c -o build/nested-domain-dispatch-test
cc "${san[@]}" ../krnl/tests/nested_domain_profile.c ../krnl/nested_profile.c ../krnl/nested_vmx.c -o build/nested-domain-profile-test
cc "${san[@]}" -DVMX_NESTED_DOMAIN_TEST -I"${EFI_INC:-${EFI_PREFIX:-/usr}/include/efi}" -I"${EFI_INC:-${EFI_PREFIX:-/usr}/include/efi}/x86_64" ../krnl/tests/nested_domain_memory.c ../krnl/nested_domain_memory.c ../krnl/nested_ept.c -o build/nested-domain-memory-test
for t in nested-domain-test nested-domain-dispatch-test nested-domain-profile-test nested-domain-memory-test; do ASAN_OPTIONS=detect_leaks=0 "build/$t"; done
# Tie the private-counter fixture test to the adapter's completion address.
grep -Fq 'if(*(uint64_t *)ram(f,config_gpa+0x820)!=2)break;' ../krnl/nested_guest.c
! grep -Fq 'if(*(uint64_t *)ram(f,0xc820)!=2)break;' ../krnl/nested_guest.c
cc "${san[@]}" ../krnl/tests/nested_owner_pool.c ../krnl/nested_owner_pool.c -o build/nested-owner-pool-test
ASAN_OPTIONS=detect_leaks=0 build/nested-owner-pool-test
tests/nested35a.sh
cc "${san[@]}" -DVMX_NESTED_DOMAIN_TEST -DVMX_NESTED_OWNER_AD_TEST ../krnl/tests/nested_owner_ad.c ../krnl/nested_owner_ad.c ../krnl/nested_owner_pool.c ../krnl/nested_ept_sparse.c ../krnl/nested_ept_atomic.c ../krnl/ad_updater_gate.c -o build/nested-owner-ad-test
ASAN_OPTIONS=detect_leaks=0 build/nested-owner-ad-test
cc "${san[@]}" ../krnl/tests/nested_domain_maintenance.c ../krnl/nested_domain.c ../krnl/nested_domain_dispatch.c ../krnl/nested_vmx.c -o build/nested-domain-maintenance-test
ASAN_OPTIONS=detect_leaks=0 build/nested-domain-maintenance-test
make smp-nested-ad-reset
for obj in build/k-nested35c-guest.o build/k-nested35c-adapter.o build/k-owner-ad-sparse.o build/k-owner-pool.o; do
 nm -u "$obj" >"$obj.undefined"
 ! grep -E ' (pmm_alloc_page|pmm_free_page|log_text|log_hex|panic|vmx_guest_enter)$' "$obj.undefined"
done
nm -u build/k-nested35c-adapter.o >build/nested35c-dispatch-symbols.log
grep -q nested_domain_dispatch_execute build/nested35c-dispatch-symbols.log
grep -q nested_domain_dispatch_physical_enter build/nested35c-dispatch-symbols.log
grep -q nested_domain_dispatch_physical_leave build/nested35c-dispatch-symbols.log
set +e
QEMU_CPUS=2 ESP_DIR=build/smp-nested-ad-reset-esp timeout 90 scripts/run.sh smoke >build/nested35c-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'SMP.N20_BLOCKED CPUID_VMX0' build/nested35c-tcg.log
! grep -q 'SMP.N35C_PASS\|PANIC' build/nested35c-tcg.log
echo 'PASS N35C domain dispatch/mapping/profile/TCG skip; shared-domain hardware requires KVM'
