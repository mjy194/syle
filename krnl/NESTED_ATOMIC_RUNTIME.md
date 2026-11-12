# N41C: bounded atomic catalog VMX runtime

N41C is a separate `ATOMIC_CATALOG` profile. It connects the N41B production
`tr_atomic` lifecycle/refill bridge to actual VMCS01/VMCS02 entry, reason-48
handling, owner-private sparse caches, local INVEPT and eight guest checkpoints.
It does not replace the legacy N35 atomic runtime or the N40 AD0 catalog profile.

## Source authority and ownership

`nested_atomic_memory_prepare` accepts only the real, fresh, started allocation
created by `nested_domain_memory_prepare`. Here `backing.started` means that the
fixture owns its allocation, not that a guest is running. The constructor audits
all 83 backing and map pages for distinct ownership, actual writable WB host
mapping and WB physical memory. The five retained EPT map trees are checked
in full, including unused paths. Every guest alias of a source table is RO.
Private pool pages and both VMCS pages are separately checked for disjointness
from that retained inventory and from one another.

The immutable WB/IPAT1/AD1 source catalog publishes exactly three full EPTPs:
A `0x1005e`, B `0x1405e`, and C `0x1805e`. A and B have separate top-level roots
and share their lower tables. C has a distinct path and target data page. All
post-construction source-table accesses, including audit reads, use 64-bit
atomics. No AD0 catalog or ordinary reader is published over this storage.
The source authority rechecks the registered span, identity, object, lifetime,
HPA, role, permissions, IPAT, host mapping, whole inventory and exact immutable
structure. Only architectural A/D changes are permitted by the structural audit.

Two fixed physical CPU owners, 0 and 1, each hold a private budget-two AD1 EPT02
cache. The source and updater gate are shared, while cache roots, trace storage,
root bindings and durable receipt slots are owner-private. Trace and receipt
storage is bounded and never silently recycled.

## Entry, faults and stopped consumers

Every physical entry, including each internal resume after a reason-48 exit,
reads the actual current VMCS and EPTP before `nested_vcpu_authorize` sets busy.
The atomic entry guard validates current CPU, stopped physical consumer, active
and hardware-pinned item, source session and saved ticket, typed token, root
allocation lifetime, cache generation, catalog/gate identity and residency.
It then performs a final revalidation after the unlocked authority callback.
Failed operation cleanup remains durably represented and denies later entry.

A stopped consumer means `vcpu.busy == false` and the current domain dispatch
slot's `guest_running == false`, with the owner token still valid. It does not
mean `nvmx.model.running == false`: an intercepted EPT fault has stopped the real
consumer while the virtual model correctly remains in L2. Physical suspend,
completion, atomic refill/promotion, revalidation and physical resume preserve
that distinction.

The fault adapter checks qualification access bits, current private permissions,
GLA-validity relationships and defined unsupported events before the production
bridge can touch source A/D. Guest paging-structure accesses retain hardware's
read+write semantics under EPT A/D. An unexplained HIT fails closed. Guest RIP is
not advanced to simulate a successful load or store; the actual faulting
instruction retries.

The fixed physical profile disables mode-based execute control and supervisor
shadow stacks. Qualification bits 6 and 14 are therefore undefined, not required
zero. Bits 9:11 are supplemental information, undefined outside their specified
translated-linear case; they do not confer refill permission. Bits 63:17 are not
currently defined. These fields are ignored, while defined unsupported events in
bits 13, 15 and 16 reject. The caller reads actual VMCS pin controls on the exit:
bit 12 rejects when defined (NMI exiting disabled or virtual NMIs enabled), and
is ignored when undefined (NMI exiting enabled without virtual NMIs). Valid IDT
vectoring is rejected before this adapter. Access must be nonzero, bit 8 requires
bit 7, a paging-structure access must request both read and write, some requested
access must be denied, and the reported permissions must match the current
private tree. All source, owner, stopped-consumer and bridge guards still apply.
See Intel SDM Vol. 3C, order 326019-078US (December 2022), Table 28-7 and section
28.2.3: https://cdrdv2-public.intel.com/671506/326019-sdm-vol-3c.pdf.

Each owner has one first-rejection diagnostic record. It retains reason 48, raw
qualification, GPA, actual pin controls, rejection stage, whether private
permissions were obtained, those permissions, and bridge status when called.
The valid flag is published last with release ordering, and later failures do
not overwrite it. It is owner-local diagnostic evidence only, never admission,
retirement, source-write or cleanup authority. A rejected adapter call also
writes those fields into the existing owner trace before teardown can replace
the visible failure with a retention fault. Successful operations leave the
record invalid. VMREAD or caller-state failures before the adapter are outside
this record's scope.

## Eight real guest checkpoints

Each owner follows A / A / B / B / A / B / C / A. Host barriers observe stopped
consumers at each VMCALL and release the next guest stage only after both arrive.

1. A read: shared source A becomes 1, D remains 0, both private target leaves RO.
2. A first write on owner 0: shared D becomes 1, owner 1's private leaf stays RO.
3. B read: both B leaves stay RO despite shared lower-path D already being 1.
4. B first writes: each owner must promote its own private leaf independently.
5. Warm A: allocation lifetime is unchanged; owner 1 performs its own first write.
6. Warm B: allocation lifetime is unchanged and prior data is checked again.
7. C: distinct backing is read and written, creating a third root and one eviction.
8. Recreated A: original data persists, the new private leaf is RO, the allocation
   lifetime differs, and each owner has four creations and two evictions.

Each of the six target first writes must pass through a private-cache promotion.
Four warm selections reuse their exact allocations. Each owner retires four
roots, including final teardown. Before private pages can be returned, the local
provider validates the complete EPTP and owner scope, performs physical INVEPT,
commits a durable typed receipt, and releases the persistent source session.
Existing source backing itself remains retained after catalog and updater drain.

Checkpoint barriers do not prove a causal provider-overlap schedule or a hardware
CAS collision. There is no A/D clear/retrack cycle, cross-domain source access,
cache-type transition, remote invalidation or source backing reclamation. The
old backing-release API continues to reject the atomic profile.

## Build and evidence boundaries

Run `efi/tests/nested41c.sh` for the complete host suite, build and TCG check.
The `host` and `runtime` arguments run just those named portions; a partial run is
not a complete top-level acceptance. In this cloud workspace first source
`efi/scripts/cloud-env.sh`. The build target is `smp-nested-atomic-catalog` and the
ESP is `efi/build/smp-nested-atomic-catalog-esp`.

The host suite includes memory authority checks, current and legacy profile
checks, preprocessed actual runtime wiring checks, and independent production
entry/dispatch/fixture/adapter slices with weakening mutants under
O2/Werror/ASan/UBSan. Host callbacks for physical state and INVEPT are mocks.
The structural integration test is not execution evidence. LeakSanitizer is
disabled for intentional fail-closed retention and container restrictions;
AddressSanitizer and UndefinedBehaviorSanitizer remain enabled.

TCG must exit 33 with `SMP.N20_BLOCKED CPUID_VMX0`, no panic, no N41C checkpoint
and no N41C pass marker. That is a capability skip, never a VMX pass. Exact-image
hardware acceptance and a fresh complete 86-suite regression are separate stages.
The predecessor's immutable N41B evidence is preserved, not reused as N41C proof.

The earlier N41C image passed its software regression but failed two exact-image
KVM runs before checkpoint 0. A later read-only snapshot established a rejected
first BSP reason-48 operation followed by fail-closed teardown retention. That
image's actual raw qualification was not captured and remains unknown. Synthetic
production-adapter cases demonstrate the old undefined-bit rejection defect;
they do not establish that it caused the observed hardware failure. The corrected
decoder and diagnostic require their own exact-image hardware acceptance and
fresh regression. Earlier images and their success/failure evidence are retained.

The frozen package README records the exact source, kernel and ESP hashes, the
checks actually completed before freeze, hardware commands/required markers and
the still-pending stages. Hardware must run those exact frozen bytes rather than
rebuilt working artifacts.

## Verified qualification candidate (2026-11-10)

The corrected candidate completed both separate acceptance stages:

- Frozen source manifest: `965d9a4777595905193ea465a4f1dd51921861d8394d42c01fd9f0489b48de13`.
- Frozen kernel and ESP kernel: `023cc6a8950fc4d242d541dc275404df1098875b2d50f7f955ba60223a46c39c`.
- Candidate archive: `frozen-n41c-runtime-965d9a477759-qualification.tar.gz`, SHA-256
  `c4baef5516a2002c10b62c5b680227d7d80d56083cbf592d64cff49bedcfcfb7`.

The fresh complete 86-suite software regression ran from 02:52:43 to 03:16:35 UTC,
with no reused acceptances. It includes the default N41C `all` invocation and
exactly the previous 85-suite inventory. The N41C checks include four actual
production slices, 25 assertion-rejected semantic mutants and 256 metadata/control
cases under O2/Werror/ASan/UBSan. Before/after guards verified all 503 source files
and 6,023 frozen/prior evidence files throughout. The independent final audit
verified all 86 fresh zero-exit records, 172 suite markers, 173 guards, ordered
inventory, timing, log offsets, hashes and the duplicate-free candidate archive.
Software evidence: `efi/build/nested41c-qualification-full-*`; independent report
SHA-256 `d234fb814150b62b5c0169c46c9f090584aafb412d6e0d90d5127095d46d6f47`.

Separate exact-image KVM runs on archbook used two CPUs at 256 MiB and 512 MiB.
Both exited 33 and passed all 44 acceptance checks. Each run recorded eight
checkpoints per owner, sixteen read+write paging-structure faults per owner,
all six summary markers, the exact `SMP.N41C_PASS` marker and then
`STAGE0.SMOKE_PASS`. No rejection, panic or capability-skip marker appeared.
The run copies, frozen package and original firmware hashes remained unchanged.
The 50-file hardware evidence archive is
`archbook-n41c-965d9a477759-qualification-kvm-evidence.tar.gz`, SHA-256
`3278cc6b9491ef3ffc935462636251e138fefb9f063164bb8bf1b650d6c71e6e`.

The corrected runs retained page-walk qualification `0x683`. The earlier failed
image's actual qualification remains unknown; this observation does not establish
its historical root cause. Earlier software passes and hardware failures remain
unchanged, separate evidence. The existing exclusions on causal overlap, CAS
collisions, clear/retrack and source-backing reclamation still apply.

This verification section was appended only after the software run, independent
audit and separate hardware acceptance completed. No runtime source was changed.
The original frozen package, manifests and pre-freeze README remain untouched;
the source hash above identifies the tested pre-append source snapshot. The
append-only document change has its own before/after hashes and diff under
`efi/build/nested41c-qualification-full-documentation/`.
