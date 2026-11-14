# Final link proof and the still-blocked mapping transaction

## Isolated opt-in final ELF

`final_link.sh NEW_OUTPUT_DIRECTORY` creates a fresh temporary build view of the
real repository `efi/Makefile`, sources and link script. It selects the standard
`build/krnl.elf` target but replaces KCOMMON's k-memory.o with the localized
optional authority object and adds the WB fence primitive. This is a **default-
kernel-shaped static link proof**, not the managed Linux/OVMF profile and not a
bootable-device acceptance claim. It never overwrites efi/build, modifies default
rules, copies an ESP or launches a VM. EFI_INC can be supplied explicitly.

All objects are rebuilt with no LTO, not reused from prior binaries. The manifest
records object/source/header hashes and final ELF hash; build.log and kernel.map
are retained. Static gates require an executable ELF, no undefined symbols,
no dynamic section/relocations, no exported raw/owned/model/test entry points,
exactly one normal PMM API and authority session API, no ordinary k-memory.o,
no private API references from other inputs, no LTO sections, and the real
0x04000000 base / 8 MiB kernel window. These assertions cover the specific produced
ELF. A different managed/Linux final link must repeat them; no LTO allowance or
native runtime inference is made.

The first real link exposed an important missing-runtime dependency: assigning
an entire production-sized registry generated an unresolved memset. `vlm_init`
now checks fresh storage and initializes only small fields, retaining fail-closed
reinit behavior without that runtime dependency. Tests include dirty-slot/serial
init rejection. The corrected actual full link succeeds.

The authority is included but no device session/virtio DMA is started by this
standard kernel entry. `krnl/main.c:syle_main` disables interrupts before PMM
initialization; runtime behavior of the privileged BSP check, subsequent APIs and
changed failure modes remains unexecuted. Static symbol closure is not protection
against kernel direct writes/calls by address, NMI, other CPUs or external DMA.

## Real x86 WB publication primitive

`wb_publish.c` now supplies an actual compiler memory clobber plus MFENCE, not the
platform pilot's counter. MFENCE provides the x86 load/store ordering operation
for already valid mapped memory; the compiler clobber prevents surrounding memory
operations being moved across it. The implementation accepts no addresses and
performs no guest writes or permission checks, so it is **not a DMA callback**.

The produced final ELF is disassembled to verify MFENCE, and a host executable
calls it successfully. That proves compilation/execution of the primitive, not
concurrent guest visibility or a correct device transaction. The intended order
is all data/status/used-element stores, this fence, then used.idx, all under the
same retained mapping/session lease and stopped-guest interval. It is not a cache
flush, INVEPT, MMIO barrier contract or synchronization with arbitrary hardware
DMA. Only WB CPU-backed emulation is proposed. Non-WB and cross-architecture
support need a separate reviewed primitive. The current test platform still uses
its counter and must not be attached to a native guest.

## Actual lifecycle connection points and missing interfaces

`linux_guest_session.c` prepares LGM using `hop.memory`, then opens VMX host state,
and runs guest entry/dispatch synchronously. Its release closes host VMX state,
disables the platform and only then calls lgm_release. Device mapping leases must
be drained before that release path starts; lgm_release clears prepared/eptp
before attempting frees, so relying on its per-page release failure is too late
to preserve an active DMA transaction's state.

The existing sg_memory_ops callbacks carry only a PA. A concrete future bridge
needs a unique **per-CREATE context** with immutable epoch and a PA→allocation-
generation inventory populated only by fresh authority allocations. A stale PA
from a prior create must never be resolved using a newly reused context or newer
allocation at the same PA. Contexts/retired entries cannot be recycled until all
references are revoked; do not invent generation capability from a PA lookup.
Map/release callbacks must validate the exact current context and retained token.
There is no such bridge installed by this work.

`hop.memory` also allocates VMXON/VMCS through linux_vcpu_host.c. Thus the initial
32,835-record guest-RAM+EPT budget needed two more records. The candidate has now
been enlarged to 32,837 for those controls; other managed buffers still need
additional budget.
More importantly, owner alone is not allocation purpose: the authoritative
record needs distinct guest-RAM, EPT-table, VMX-control and host-metadata types.
DMA mapping admission must accept only guest-RAM, never merely any allocation
belonging to the VM. The authority now implements typed allocation records and a separately tested
per-CREATE token context (see CREATE-CONTEXT.md), but actual role-specific LGM/VMX
call sites are not connected. It cannot authorize DMA just because its
owner/generation/type checks pass.

The minimal mapping transaction still requires all of:
1. Session/device lifecycle lock and real vCPU/DMA writer exclusion, not only IF=0.
2. Authoritative typed-allocation query and pin for every page, consistent with
   the GPA ledger and the required actual EPT paths/mapping inventory.
3. Full spans pre-mapped/pinned before mutation, with stable WB mappings and
   immutable-disk/metadata alias exclusions. The existing per-page audit does
   not prove global EPT alias freedom or another VM's absence.
4. Snapshot and execute under a single immutable epoch; real fence before idx;
   retain ownership through outer notification RIP commit. Any uncertain outcome
   is terminal, never retried or rolled back.
5. Explicit drain and release in lifecycle order; failed page release retains the
   exact capability and blocks session end. No live context reset, partial teardown
   or release behind a pinned mapping.

Typed allocator/mapping authority, exclusion/TOCTOU handling, lifecycle wiring,
actual DMA publication and hardware tests remain blocking work. This document
and the final-link artifact do not enable the device or claim Linux storage.
