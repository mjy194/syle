# Concrete opt-in integration sequence (not yet enabled)

## Current evidence and recommendation

Use the immediate-release, no-within-CREATE-reuse context for the first fixed-LGM
experiment. LGM currently allocates all RAM, then its EPT tables, and releases at
teardown; it does not need a general host heap inside an active CREATE. Keep the
deferred alternative separate until a concrete legitimate churn requirement is
measured, since it changes release accounting and requires finalization ownership.
Neither context is currently registered in the real manager.

The new `managed_final_link.sh NEW_OUTPUT_DIRECTORY` builds the actual
managed-host-clock-24 target in an isolated directory, replacing PMM only and
adding the unused fence primitive. It still uses existing **untyped** callbacks
from `linux_guest_manager.c` and has not booted. Input fixture validation is run
by the original MB check target. A passing link is not typed allocator adoption,
virtio registration, or Linux storage success.

## Ordered next patches and exact acceptance gates

1. **Keep the profile additive.** Add an explicit external make fragment/target
   using the managed baseline object set with a unique object prefix. Substitute
   only the localized authority PMM object; never link ordinary k-memory.o too.
   Rebuild all inputs no-LTO and run final ELF gates. Do not modify `all`, default
   CPU/#PF profile, existing baseline input hashes or firmware observer files.
2. **Typed allocation calls, not inferred labels.** Add a role-aware opt-in LGM
   preparation path: RAM allocation sites explicitly request GUEST_RAM and EPT
   allocation sites EPT_TABLE. VMXON/VMCS allocation in linux_vcpu_host's opt-in
   path requests VMX_CONTROL. Do not infer role from an unaudited allocation count
   or accept guest-provided purpose. Preserve the original prepare/release path
   and its failure-token semantics for default profiles. Add failure tests at
   each role boundary and verify no map/zero occurs before capability admission.
3. **Unique CREATE context.** `linux_guest_manager.c` currently supplies static
   PA-only memory_ops, so replace those only for the new profile with controller-
   owned epoch context and immutable capability inventory. Never reset/reuse that
   context while an old callback can survive. Before exposing a PA, store its
   generation and role. A stale old context must reject a physically reused page.
   Existing `sg_memory_ops.map` used during initialization is NOT a DMA mapping
   permission; CPU initialization needs EPT/VMX access which a device must not get.
4. **Explicit mapping leases.** Add a separate device mapping API that resolves
   only GUEST_RAM, pins exact allocation tokens, verifies WB native mappings,
   excludes disk/control/metadata aliases, and retains every span through commit.
   Validate actual EPT/lifecycle ownership; current per-page audit is insufficient
   to prove global aliases/sharing. Fail mapping of any page before any device
   write, or terminally retain state if an unexpected late failure occurs.
5. **Synchronous transaction admission.** Allow one request while in the known
   VM-exit dispatch path only, with guest execution and all other writers excluded.
   Do not turn CPL0/BSP/IF0/no-AP checks into a false global quiescence proof. The
   controller must establish that no vCPU, device worker, NMI path or external
   DMA targets these pages. MFENCE orders admitted WB CPU writes, not hardware
   ownership. Preserve data→status→used element→MFENCE→used.idx and hold leases
   through final IO/RIP commit. Failure/uncertainty forbids resume and retry.
6. **Lifecycle teardown before physical cleanup.** First disable new notifications
   and acquisitions, stop execution/writers and drain request work. Only the real
   controller can establish VMCS/EPT/global dependency quiescence. Existing
   lgs_session release closes host VMX, disables platform, then lgm_release. Any
   additional pin/context cleanup must be ordered with those proofs. Never call
   partial context finalization as if it were an all-leases/global barrier:
   unpinned EPT/VMX pages can otherwise free while RAM remains pinned. Retain exact
   generation records on any release failure; no manager success/new CREATE until
   cleanup actually completes. Deferred logical retire needs a new mandatory
   physical-finalize stage, not silently swapped sg_memory_ops.release semantics.
7. **Only then register the device.** Complete real PCI function/BAR routing,
   full ring ownership and IRQ support before promising Linux. The current
   transport remains a single-head polling profile, not production virtio. Keep
   firmware polling and Linux interrupt milestones separate.

## Memory and artifact budget

Managed-profile static replacement currently fits in the existing 8 MiB kernel
window. One measured artifact occupies 4,419,584 memory bytes. A 32,837-entry
context with the current 64-byte entry needs another 2,101,568 bytes; their sum is
6,521,152, an estimate only until actually linked with that inventory and all
additional code/data. The exact linker ASSERT must remain enabled after every
change. PMM must also have enough free eligible pages for the 134,500,352-byte
RAM/EPT/VMX page budget, separate from kernel metadata and disk backing.

**Do not embed a whole disk image into the 8 MiB kernel window.** Plan a separate,
immutable, owned disk backing allocation loaded/verified as a dedicated boot input
(or copied into separately allocated pages under a bounded loader contract).
The loader/boot-info reservation and source hash/size must be explicit, and disk
pages must not enter guest DMA RAM mappings. Loading that input and authenticating
its contents remain unimplemented. Enforce capacity = checked length / 512;
reject zero/non-sector sizes, overflow and aliases. No host block-device writes.

## Current native guard and its limits

The candidate now checks CPL0 and the existing paging_ap_start_attempted marker,
as well as IF=0 and BSP, before allocator/session operations. This reuses the
same no-AP-start constraint already present in linux_vcpu_backend execution checks.
It remains a narrow admission guard, NOT proof of NMI exclusion, device DMA
quiescence, mapping lifetime or actual runtime correctness. Host tests mock the
guard. No native execution or automatic promotion is authorized by these tests.

## Retained proof and replay

The managed static proof now retains a manifest, exact copied kernel/initramfs
inputs, toolchain versions, build log/link map and a 308-file source/control/header
snapshot. A replay using only that snapshot and copied inputs completed all final
ELF gates. The two full ELF hashes differ because build/debug paths differ, but
extracted `.text` + `.rodata` + `.data` binaries compared byte-for-byte equal and
kernel memory extent matched (4,419,584 bytes). This is reproducible static code/
data evidence, not execution of privileged code, typed callbacks, virtio or Linux.
Use the manifest/replay-evidence in the retained output; never promote this to a
boot claim or assume an image with later source edits is the same artifact.
