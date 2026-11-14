# M2 Linux EPT contract and acceptance boundary

## Policy: guest permissions are not host ownership

The current direct-Linux platform owns 128 MiB of RAM, addressed by guest physical
addresses (GPAs) `[0, 0x08000000)`. `linux_guest_memory.c` creates a four-level EPT
with 32,768 4 KiB leaf entries. Each leaf points to that guest's allocated backing
page and uses `0x37`: R/W/X plus write-back memory type. Unused EPT entries are zero.
The EPTP uses write-back memory and four-level walking (`root | 0x1e`).

RWX here is the intended *physical RAM access policy*, not an assertion that
Linux code, data and stack all have executable guest virtual mappings. Linux's
own page tables enforce its virtual permissions. Ordinary guest `#PF` is native
by default, so Linux can implement demand paging and COW itself. A guest #PF is
not an EPT violation. Do not reintroduce an error-code whitelist to protect host
memory, and do not require all guest RAM to be W^X without a separate use case.

The isolation requirement is that guest physical access cannot resolve to host
code/data, VMX control structures, EPT tables, or another guest's allocation.
No current claim covers DMA/IOMMU isolation or physical passthrough.

## Trusted allocator and caller boundary

`linux_guest_memory.h` explicitly requires `allocate` to return distinct,
exclusively owned pages. `lgm_prepare` checks basic token shape, WB accessibility
and mapping, but **does not independently reject duplicate RAM pages or a RAM
page repeated as an EPT-table page**. These are violations of its trusted
allocator contract, not supported adversarial inputs. The tests' distinct
allocations do not prove robustness against a malicious allocator.

`lvh_open` additionally rejects newly allocated VMXON/VMCS pages that overlap
RAM, EPT tables or each other, before mapping/zeroing them. It quarantines such
cross-owner overlap instead of freeing an unowned page. This defense does not
turn `lgm_prepare` into an arbitrary-untrusted-allocator verifier.

`lgm_release` itself cannot know whether a vCPU still references the pages. Once
installed, callers must complete VMCLEAR, INVEPT and VMXOFF, and prove no live
vCPU reference before release. Failed release tokens are retained for retry.
The managed session/host lifecycle supplies this ordering; calling release on
active memory directly is outside its contract.

## Executable host checks

Run from the repository root:

```
sh krnl/tests/linux_guest_memory.sh
sh krnl/tests/linux_guest_memory_walk.sh
sh krnl/tests/linux_vcpu_prepare.sh
sh krnl/tests/linux_vcpu_host.sh
sh krnl/tests/linux_vcpu_host_alias.sh
```

- `linux_guest_memory`: actual memory adapter, full RAM initialization/load
  comparison, every leaf, unused upper-level entries, allocation/map/WB failure
  boundaries and failed-release ownership retention. An optional two-file
  invocation loads a supplied bzImage/initramfs, but still does not execute it.
- `linux_guest_memory_walk`: a separate four-level walker traverses the actual
  adapter's EPT using a scattered trusted backing permutation. Every GPA page
  is checked at offsets 0, 1, 2047 and 4095 against its specific allocated HPA;
  representative addresses outside RAM (including upper-level boundaries and
  beyond 48-bit GPA) have no mapping. RAM/table ownership is distinct under the
  trusted allocator fixture. This is a **software structural walk**, not VT-x.
- `linux_vcpu_prepare`: initial VMCS/profile and physical-address constraints.
- `linux_vcpu_host`: ordered host/VMX setup and teardown, simulated failures,
  retention and retry. No actual VMX instruction executes in this test.
- `linux_vcpu_host_alias`: cross-owner RAM/EPT/VMX-page overlap is rejected before
  map/zero/free; quarantine prevents reuse. Again this is host-only evidence.

Current shell and external terminal KVM acceptance proves the intended Linux
mapping supports that workload and final cleanup. It is not an adversarial
physical isolation or R/W/X permission fault-injection matrix.

## Remaining hardware acceptance

The following should be separately named and run against the *actual managed
Linux memory/lifecycle path*, not inferred from a historical small diagnostic
VM or this host walk:

1. Record GPA-to-owned-HPA mapping provenance and verify RAM bounds. Use a
   controlled guest access outside permitted RAM; require an expected EPT exit
   and fail-stop without exposing host bytes or changing mappings silently.
2. Reserve a safe test page and define an explicit diagnostic permission-change
   contract. Remove R/W/X individually, cause the corresponding guest access,
   verify actual exit reason 48 and its qualification/GPA, restore only that
   page's permission, INVEPT, then retry the same instruction exactly once.
   This must not become the normal guest PF path or mutate live Linux code.
3. Test STOP -> START state preservation and DESTROY reclamation. Repeatedly
   create/run/stop/destroy; ownership/resource counts must return to baseline.
4. Keep representative host cleanup-failure injection cases, and record which
   failures are modeled versus actually induced on KVM.
5. Re-run Ring3 isolation negative cases separately; manager isolation and guest
   EPT isolation are different boundaries.

The current normal Linux dispatcher does not promise generic EPT permission
repair/retry; its reason-48 path primarily serves the optional APIC MMIO route.
Do not mark dynamic R/W/X hardware acceptance complete because the EPT leaf
structure is correct. Conversely, do not label deliberate private-RAM RWX as a
confirmed vulnerability without identifying an actual ownership violation.

Firmware work will add ROM/MMIO regions and new device access rules. That is a
new address-space profile requiring its own ownership and permission tests;
these direct-Linux bounds must not be silently generalized to that profile.
