# N47A: retained stopped-domain source replacement

Scope: two fixed owners, WB four-level/4KiB, IPAT1, AD1. One cooperative
full-checkpoint transaction replaces one shared A/B EPT12 leaf with a different
already-owned WB data page. Numeric virtual EPTP and L2 GPA stay unchanged;
C's control leaf is unchanged. All source tables and all old/new data remain
owned and retained permanently. Only successfully retired private EPT02 pages
return to their original owner pool. This is not general dynamic EPT, live
source mutation, source/data reclamation, arbitrary guest input or hardware
verification. No EPT01/type/PAT/MTRR change, WBINVD or INVVPID is added.

## Production boundaries

- `typed_root_source_replace.h` and `.inc` implement the independent one-shot
  coordinator, compiled inside `typed_root_atomic.c` to share the production
  identity predicates. It cannot coexist with TAM/AD_RESET on the same owners.
- The read-only plan_close hook runs only after both real consumers finished
  cooperative checkpoints and their owner adapters finished exit reflection.
  It returns the complete expected SOURCE_REPLACE domain ticket without closing
  the gate. The coordinator durably binds it in BOUND before close executes and
  verifies that exact ticket. BOUND routing accepts original OPEN or only that
  exact maintenance ticket, covering the complete physical-close handoff. Normal atomic entry/fault/registry
  admission closes separately. Exact stopped routing applies from BOUND onward,
  permitting operations admitted before close to complete their final guards.
- Directory/lease admission stays open for retirement. Old updater is closed
  and drained only after operations drain; old catalog admission then closes.
  Existing persistent pins remain valid until the normal `before_forget` chain.
- Park/restore/complete hooks are privileged-adapter contracts. Park audits
  VMCS01/02, performs actual parking EPTP write/readback before precise unpin,
  and preserves launch state. Restore receives an already-pinned new root;
  uncertainty retains that pin. Complete revalidates actual per-owner EPTPs and
  launch state before domain ACK/publish/complete. N47A mocks these operations.
- Internal one-use commands bind owner, nonce, kind and concrete key or stable
  full root identity. Begin consumes them before platform callbacks, reserve
  and select validate the actual key, and end verifies exact pin change.
  A phase does not grant ordinary SELECT/PIN/UNPIN/DISPOSE permission.
- Every old active/inactive root follows real sparse flush, extra owner-local
  provider INVEPT, durable retirement receipt, typed token retirement, source
  pin end, forget, then private page return. Old evidence never dereferences a
  freed item: it uses stable slots and copied identities. Naked item pointers
  do not provide stale-lifetime protection and are not claimed to do so.
- Successful registry destruction zeroes that registry. The coordinator rearms
  only its saved allocator/hook/configuration with its monotonic LRU tick.
  Owner, bridge, provider, directory, lease, slots, bindings, traces, receipts
  and completed evidence are never reset or recycled.
- `nested_source_replace_memory.[ch]` independently validates retained raw
  source graph and management authority after drain. It attempts one exact
  old-word CAS, clears only target leaf A/D, preserves every other source word
  including A/D and ignored payload, and supplies no release/rollback API.
  TABLE aliases must be guest-RO; replacement DATA requires explicit owned WB
  non-code/non-stack/non-table attestation, but may have writable guest aliases.
- `tsr_replace`'s trusted change callback composes that memory operation with
  successor construction/publication. Its contract is exactly one audited
  leaf change, not arbitrary source-write authority. It must return both new
  authority contexts. The coordinator verifies the predecessor bank bindings,
  nonce continuity, unchanged span allocation lifetimes/mappings, every source
  generation increment, full new identities and optional wrapper-config mirror.
- Catalog/updater successors consume closed+drained predecessors once, under
  each predecessor's lock, and inherit their independent final nonce highwater.
  Old updater is terminal. This prevents old NAC tickets matching a new lease
  at the same `atomic.fault.permit`/session/token storage address.
- Final release and abort share the original metadata lock. Owners must wait
  for exact `tsr_released`, not merely domain OPEN. Abort changes only shared metadata under its lock; owner-local registry
  fields are not written remotely. New gate pointers are snapshotted under the
  lock before poisoning. New source pins/open catalog and exact updater stamp
  are revalidated before and after the physical completion callback.
  An abort that wins before release permanently poisons; a stale abort after completed release is ignored.
  There is no retry/reopen after an uncertain started action.

SOURCE_REPLACE dispatch begin is purpose-specific and validates both fixed
active/current/launched owners, history3, no prepared/running physical consumer,
no local invalidation or internal suspension before issuing a ticket. It cannot
inspect private `nvmx.running`: the owner adapter must finish
`nvmx_return_from_l2` before the close hook. AD_RESET behavior is unchanged.

## Focused software validation

Run from repository root:

```
python3 efi/tests/nested47a_core.py --sanitizer none
python3 efi/tests/nested47a_core.py --sanitizer address,undefined
python3 efi/tests/nested47a_core.py --sanitizer thread
python3 efi/tests/nested47a_successor.py --sanitizer none
python3 efi/tests/nested47a_successor.py --sanitizer address,undefined
python3 efi/tests/nested47a_successor.py --sanitizer thread
python3 efi/tests/nested47a_dispatch.py --sanitizer none
python3 efi/tests/nested47a_dispatch.py --sanitizer address,undefined
python3 efi/tests/nested47a_dispatch.py --sanitizer thread
python3 efi/tests/nested47a_memory.py
python3 efi/tests/nested47a_mutants.py
python3 efi/tests/nested47a_race_mutants.py
source efi/scripts/cloud-env.sh
python3 efi/tests/nested47a_freestanding.py
```

Core runs actual catalog/NAC/typed bridge/registry/pool/provider code with a real
pthread metadata lock and TLS owner identity. Two owners concurrently retire
and build roots. Forced pauses cover admitted fault, provider, allocation,
source cleanup and entry cancellation. A strict dispatch-state stop mock
pauses physical close after domain closure while an admitted fault completes.
A remote abort is raced against the owner registry path with relaxed scheduling
flags that do not manufacture a happens-before edge. Final domain-complete/abort
is raced after the final updater snapshot and before release. The same pool root HPA and initial cache generation are reused with
new allocation identity, and actual `atomic.fault.permit` storage is attacked
with old NAC/NRC/updater tickets plus old typed tokens and traces. Ten failure
paths retain-stop, including receipt committed but source pin end failed and
second-owner restore uncertainty. Source/data have no free path.

Ten isolated mutants must compile and die by a runtime assertion: missing
nonce continuity, skipped drain, forged local INVEPT, early pin release,
ignored receipt identity, missing second owner preparation, ignored late abort,
failure reopening, a missing close handoff route and skipped final new-bank guards. A first-running successor test deliberately makes both
old ticket numbers 1 so missing seeding revives the exact old NAC ticket.

Dispatch TSan is functional single-thread coverage; memory TSan is likewise
functional, not race evidence. Core and successor suites use actual threads.
Freestanding validation builds the retained WB Makefile kernel/ESP with new
coordinator/successor/dispatch symbols, checks no unresolved/libc symbols, and
separately compiles the management graph module. It does not wire or execute
N47B. All failing development attempts are retained separately.

N47B remains pending independent N47A review: actual parking/readback, fixed
VMCS audit, both owners' real initial/retirement INVEPT, old/new sentinel data,
C unchanged, first-read RO and each owner first-write AD, retained backing and
unchanged launch state require their own exact-image hardware evidence.

## Review revisions

Frozen v1 passed its focused matrix but was not accepted. Independent review
reproduced two real gaps: the old OPEN-only stopped route during close handoff,
and a TSan race from remote writes to owner-local registry poison fields. A
third probe showed missing final new-updater state revalidation. v2 adds the
prebound exact ticket transition, lock-disciplined shared-only abort publication,
locked atomic transaction-end health handling, and pre/post-completion bank
checks. Dedicated strong schedules and failure probes cover all three fixes.
V1 source, failed attempts and independent reproductions are retained.

V2 independently fixed all three v1 issues, but a further TSan schedule found
pre-drain budget reads of owner-local registry tick. V3 separates shared health
from owner-local health. During an admitted operation, close reads only shared
metadata/immutable source counters and defers owner registry/binding/trace scans
until td_end and an empty operation ledger prove their writers completed. Full
local checks occur only at exclusive init, after drain, on the current owner,
or at final prepare completion while ordinary admission is still closed.
Released polling never reads peer-local registry fields; the owner publishes
local failure through the shared health record at transaction end. Strong
relaxed schedules cover warm SELECT's last guard/tick gap, pending end and a
normal owner failure while a peer still queries release. Three separate race
mutants must produce real TSan data-race reports (exit 66), in addition to the
ten assertion-killed mutants. V1 and V2 remain unchanged and blocked.
