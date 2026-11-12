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
make smp-nested-domain
for obj in build/k-nested33b-guest.o build/k-nested33b-adapter.o; do
 nm -u "$obj" >"$obj.undefined"
 ! grep -E ' (pmm_alloc_page|pmm_free_page|log_text|log_hex|panic|vmx_guest_enter)$' "$obj.undefined"
done
nm -u build/k-nested33b-adapter.o >build/nested33b-dispatch-symbols.log
grep -q nested_domain_dispatch_execute build/nested33b-dispatch-symbols.log
grep -q nested_domain_dispatch_physical_enter build/nested33b-dispatch-symbols.log
grep -q nested_domain_dispatch_physical_leave build/nested33b-dispatch-symbols.log
set +e
QEMU_CPUS=2 ESP_DIR=build/smp-nested-domain-esp timeout 90 scripts/run.sh smoke >build/nested33b-tcg.log 2>&1
status=$?
set -e
[[ $status == 33 ]]
grep -q 'SMP.N20_BLOCKED CPUID_VMX0' build/nested33b-tcg.log
! grep -q 'SMP.N33B_PASS\|PANIC' build/nested33b-tcg.log
echo 'PASS N33B domain dispatch/mapping/profile/TCG skip; shared-domain hardware requires KVM'
