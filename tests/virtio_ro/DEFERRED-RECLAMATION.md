# Alternative for legal PA churn: defer physical reclamation to epoch end

This is a **separate experiment** (`deferred_context.*`), not a modification or
replacement of the reviewed `create_context` implementation. It uses the same
actual optional authority under host tests; it does not manufacture ownership
from caller labels, install sg_memory_ops callbacks, map memory or enable DMA.

## Why a bound is unavoidable

A callback taking only (CREATE context, PA) cannot distinguish a delayed old use
from a legitimate new use after the same PA is recycled within that context.
If old calls must remain rejected without generation-bearing arguments, the PA
cannot be re-exposed in that context. With finite physical pages, unlimited churn
and indefinite stale-reference protection are therefore incompatible. Owner IDs
or an allocation generation hidden from the callback do not remove this ambiguity.

Three concrete choices are:
1. Current reviewed pilot: free immediately, retain a tombstone and reject a
   later allocator result at the same PA, releasing that new token. Safe but a
   lowest-free allocator can repeatedly return the same PA and make no progress.
2. This alternative: logical retire now, physically reclaim only after epoch
   quiescence. The allocator cannot return retired pages because they remain
   protected allocations. Legal churn progresses through distinct pages up to an
   explicit lifetime budget, rather than repeatedly hitting one tombstone.
3. A new ABI carrying allocation generation on every operation, or proof that all
   old callers are revoked before starting a new context. This can permit reuse,
   but is not compatible with unchanged PA-only callbacks and live old references.

## Implemented alternative

`vdc_retire` marks a record logically dead and rejects future pin/retire on its PA,
but deliberately performs **no PMM release**. The authority still owns the exact
allocation-generation capability, so even ordinary pmm_free_page cannot reclaim
it. New allocation comes from the actual PMM and receives a distinct page while
all records remain retained. New acquisitions stop before lifetime budget or
registry capacity is exceeded. No allocation results are adopted or relabeled.

Before the FIRST `vdc_finalize`, the controller must already prove global
quiescence of guest execution, device writers and every EPT/VMX/dependent resource.
The function does not establish that barrier. It begins drain and performs
**partial cleanup**: it releases unpinned capabilities (including retired pages)
while skipping pinned ones. For example, unpinned EPT/VMX pages may be released
while a RAM pin remains, so per-page pins cannot replace global dependency
quiescence. A false result can mean some resources have already been physically
released; the guest must not resume and the context must remain for cleanup. Successful releases are marked
complete and never retried; failed releases remain present and prevent session
end. Partial admission failure also retains its token even if no PA was returned.
A new CREATE may reuse those physical pages only under a new context/epoch; old
context methods reject them. Types still come from fresh authority allocations
chosen by trusted role-specific call sites, not guest input or external labels.

Fresh-zero, stable, disjoint context/inventory storage and stopped-guest/exclusive
lifecycle conditions are still trusted API requirements. This is not a general
concurrent allocator, mapping registry or hardware protection boundary.

## Exact resource bound

For budget B, at most B successful or retained-partial allocations can occur in
one epoch. Each retained allocation consumes one authority slot, one inventory
entry and 4096 bytes of PMM RAM. Retiring a page does not replenish that budget.
The next allocation fails visibly with no output/accounting change at the bound.
Worst-case retained physical cost is 4096*B bytes, regardless of current logical
live set; metadata adds B*sizeof(vdc_entry) plus authority records.

The known fixed LGM/VMX preparation needs 32,837 pages (RAM+EPT+two VMX controls).
A budget of exactly that size leaves **zero extra host-allocation churn headroom**.
Supporting Δ extra allocation events during the same CREATE requires Δ extra
slots/records and up to 4096*Δ extra bytes, reserved and checked explicitly.
The existing guest Linux allocating guest pages inside its fixed RAM is not the
same as allocating new host PMM pages. For arbitrary unbounded host churn, this
alternative is not a solution; change the generation-bearing ABI/lifetime model.

## Not a drop-in release callback

`sg_memory_ops.release` currently reports physical allocator release success.
Replacing it silently with logical retire would let lgm_release clear tokens and
its owner callbacks while physical pages still exist. A production deferred bridge
must retain a separately owned epoch inventory and add mandatory finalization to
the manager release path. It must not report CREATE teardown complete until
physical finalization succeeds. Failures must retain that inventory for cleanup,
not let a new CREATE overwrite it. This semantic integration is **not implemented**.
It must also preserve VMCLEAR/INVEPT/VMXOFF ordering, mapping pins, device/CPU
exclusion and real publication ordering. None follows from deferred reclamation.

## Verified experiment

`deferred_context.sh` compiles independently with strict freestanding flags and
runs ASan/UBSan tests. Actual PMM + candidate tests exercise four sequential
allocate/pin/unpin/retire cycles: four distinct PAs, no repeated-lowest-PA failure,
all retained protected from ordinary free, fifth allocation rejected at budget,
finalization returns allocated count to zero, and a new epoch reuses the first PA
while old context calls fail. A live pin blocks finalization until unpin/retry.

Failure-mock tests cover mixed multi-entry physical cleanup: one succeeds, one
fails, retry never double-frees the successful record, and eventual release
permits end. Post-allocation lookup failure retains an unpublished token through
failed cleanup and eventual successful finalization. These are not native/DMA
visibility tests and do not relax ownership/mapping barriers.

The permanent actual-PMM test additionally holds a RAM pin alongside an unpinned
EPT page. First finalize returns false but frees the EPT page (allocated count
2→1); retry does not free it again; new allocations are rejected without changing
output; unpin/retry frees remaining RAM and ends the epoch. This fixes the partial-
cleanup semantics in a regression, not a proof of the caller's prerequisite
quiescence. The host test has no running VMX/EPT user.

Budget admission now queries `pna_capacity()` and rejects zero/over-capacity B
before reading inventory entries. Native profile is 32,837; the host proof selects
four slots. UINT32_MAX and capacity+1 are permanent rejection cases. Budget sizes
are unsigned bounded counts; at the current native ceiling physical cost is
134,500,352 bytes (32,837 * 4096), representable in uint64. No unchecked byte-count
allocation is performed here. A larger churn budget requires a deliberately
larger authority build and admission/accounting tests, not a caller override.
