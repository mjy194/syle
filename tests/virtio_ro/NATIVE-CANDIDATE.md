# Optional PMM authority candidate: compile/link gates, NOT enabled

`native_candidate.c` is an independent optional replacement PMM translation unit.
It embeds the unmodified actual `krnl/memory.c` plus the generation authority,
then exposes guarded standard PMM APIs and session-scoped `pna_*` APIs. Do NOT
link it alongside the ordinary memory.c object. No default build selects it, and
no firmware/Linux native run has used it. Run `sh tests/virtio_ro/native_candidate.sh`
for its gates; `run.sh` retains the earlier baseline experiments.

## Raw entry-point closure in the candidate object

The script compiles an ELF object with explicit `-fno-lto`, then `objcopy`
localizes every `pmm_raw_*`, `pmm_owned_*` and `vlm_*` definition. All belong to the
same translation unit, so internal calls remain resolvable while separate clients
cannot link against the symbols. `nm -g` asserts no such exported definitions;
negative link tests independently try raw free/allocate/init, owner release and
model init. No test-only hook is exported by the native object. This is stronger
than merely renaming a public free function but still a **build property of this
optional object**, not evidence of the final native binary's complete closure.

The unprocessed `.raw.o` must never enter a final link. A production build must
make the localized object mandatory, exclude ordinary memory.o and all other PMM
copies, and verify the final ELF symbol table and relocations. LTO, unusual
visibility/export maps, archive extraction and link-time instrumentation require
fresh final-artifact verification; they are not covered by this gate. Local
symbols are not a protection boundary against malicious kernel code/direct memory
writes, debug symbol lookup or arbitrary calls by address. Direct page-table
mapping and hardware DMA bypass this allocator authority entirely.

## Lifetime/session behavior

A process-lifetime static registry holds allocation generations and lease serials.
`pna_start` requires initialized PMM, no active session, no retained allocations,
nonfailed registry and an unexhausted epoch. A new epoch is issued without clearing
allocation/lease serial history. Session arguments are checked on every pna
operation. End requires prior drain, no leases and no allocation records; it does
not free anything implicitly. A new session can then start, but old session IDs,
old allocation tokens and old leases remain stale. No authority reset API exists.
Counters fail closed rather than wrap. Kernel restart/reinitializing physical
storage while external handles survive remains unsupported, not solved by epochs.

The native registry reserves 32,837 records (128 MiB of LGM RAM, 67 EPT pages and two VMX controls),
about 1.25 MiB of static authority storage. Its size is checked in the compiled
object. Additional owned allocations/VMs require a capacity plan; exhaustion must
not allocate an untracked PMM page. The host test selects four slots to exercise
exact capacity cheaply. This is one managed session, not a multi-VM allocator.
Metadata itself must be included in the kernel image reservation and excluded
from guest EPT/DMA mappings by the integration.

## Exclusion, cleanup and remaining blockers

The native guard checks CPL0, no `paging_ap_start_attempted()`, RFLAGS.IF clear,
then IA32_APIC_BASE.BSP before public
PMM and session operations. Only BSP + interrupts disabled is supported, matching
PMM's existing documented bootstrap contract. Native code is compiled/disassembled
but **never executed** by these tests. Host guard behavior is a test replacement,
not evidence that VMX/NMI/CPU scheduling obeys the required exclusion.

This guard is NOT a lock, vCPU stop proof, EPT lease, device-DMA exclusion, NMI
reentrancy guard or publication fence. APs/interrupt-enabled callers fail without
mutation rather than becoming safe concurrent clients. NMI/reentrant allocator
use is prohibited. Support for concurrency requires an actual common critical
section and lifecycle protocol across every client. Direct mappings/shared EPT
leaves and external DMA writers remain unguarded. Native DMA remains BLOCKED.

An interrupted/failed request must retain its lease until the guest is terminally
stopped; no retry after uncertain used.idx/RIP publication. Teardown drains leases,
then owned releases; a failed PMM free retains its record and prevents session
end. Existing VMCLEAR/INVEPT/VMXOFF requirements still apply before memory release.
There is no automatic reset/reclamation path. A real memory publication fence,
complete mapping ownership checks and device/vCPU exclusion are not implemented.

## What was tested

- Full-capacity native compile, privileged guard instruction present, no host
  hooks exported, localized private symbols and negative external links.
- Host execution uses real PMM plus mocked guard: wrong guard denies standard
  alloc variants/free and session operations without dropping live ownership.
- Active-session restart rejected; drain blocks new leases/allocations; end
  rejects live lease/records; explicit cleanup permits next epoch.
- Same-owner/same-PA generation and old-session/stale-token rejection, session
  epoch exhaustion and allocator capacity behavior; no implicit reclamation.
- Existing unchanged PMM normal/fragmentation/zero/single test modes pass through
  the candidate's public wrappers. Full earlier generation/extent-failure tests
  remain in `run.sh` and pass independently.

No final native-link, guest boot, mapping isolation, multi-CPU or DMA-publication
claim follows from these host/compile results.

## Additional candidate-composition gates

The native compile now uses the material kernel ABI flags from
`efi/Makefile` KFLAGS: `-O1 -ffreestanding -fshort-wchar -mno-red-zone
-mgeneral-regs-only -fno-stack-protector -fno-stack-check
-fno-asynchronous-unwind-tables -fno-unwind-tables -fno-pic -fno-pie
-mcmodel=small -DSYLE_KERNEL`, plus explicit `-fno-lto`. This checks the actual
candidate under the repository's small-code-model/no-red-zone convention, not
just default host ABI. It still does not prove final relocation placement,
whole-kernel link compatibility, privilege execution or effective exclusion.
The eventual complete kernel link must preserve the no-LTO contract (or establish
a separately audited LTO pipeline) and recheck private symbols/final relocations.

A candidate-specific actual-PMM fragmentation process now exhausts free-extent
metadata, verifies owner release fails with unchanged allocation accounting,
verifies the record can still be pinned and normal free is denied, then drains
and verifies session end refuses retained records. Coalescing adjacent extents
allows owner release and end; remaining ordinary allocations are cleaned up to
zero allocated pages, followed by a fresh successful epoch. This is execution of
the candidate combination with a mocked guard, not merely inference from the
earlier wrapper's tests. NMI, concurrent DMA and native publication remain out
of scope and blocked.

## Separate final-link gate now available

`final_link.sh` and `FINAL-LINK-AND-MAPPING.md` additionally cover one isolated,
no-LTO, default-kernel-shaped final ELF from freshly built objects. That narrows
the earlier unverified-final-link gap for that exact artifact only. The managed
Linux/OVMF profiles, runtime guard, mapping authority and DMA remain unverified.
