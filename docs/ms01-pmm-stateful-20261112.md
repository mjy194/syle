# MS-01 follow-up: stateful PMM public-API verification

This follows the [seven-file host/ABI supplement](ms01-followup-20261112.md).
That earlier delivery is retained unchanged. This supplement changes tests,
aggregate associations and documentation only; `krnl/memory.c` is unchanged.

## Gap addressed

The existing PMM tests cover fixed examples, exhaustion, invalid frees, sparse
maps, bounds and free-list capacity. They did not cover long interleavings of
constrained allocation and release, or allocation that must split a free extent
when all 4096 extent slots are occupied. Free-list insertion failure and
allocation-split failure are different production paths.

`krnl/tests/memory_model.c` links the real allocator and uses only its public
API. Its separate page bitmap tracks eligible/allocated ownership, without
using the allocator's extent representation or private state. Synthetic physical
addresses are never dereferenced. `PMM_TEST_PHYS_BITS=48` fixes the existing
test seam; no privileged instruction, guest, QMP or hardware mapping runs.

## New checks

- Eight deterministic seeds, each with 25,000 mixed steps: unrestricted and
  upper/lower constrained allocations, valid/unaligned/reserved/hole/duplicate
  frees, and refused reinitialization. Every operation checks statistics and
  ownership against the independent model. The model accepts any eligible free
  page satisfying the constraint; it does not require a first-fit allocation
  policy for those sequences.
- Fifteen shuffled descriptors include RAM crossing 4 GiB, sparse synthetic
  1/32 TiB ranges, low memory, firmware/protected/runtime/non-WB exclusions and
  whole-descriptor reservations for inherited stack/page tables and ACPI.
  Explicit byte-straddling loader/kernel/user/framebuffer reservations produce
  exactly 183 eligible pages. No RAM-capacity or actual high-memory access is
  inferred from these synthetic addresses.
- Full metadata: 4095 isolated free pages plus a three-page tail use all 4096
  free extents. Allocating the middle tail page would require another extent
  and must fail without changing counts or ownership. Consuming one isolated
  extent allows retry; releasing the middle page coalesces both neighbors.
  Complete reallocation verifies that every recovered page is present once.
- A test-local artificial mutation corrupts free-page statistics only when
  split allocation fails at capacity. All seven existing PMM test processes
  accept that mutant; the new capacity assertion rejects it. This demonstrates
  the specific added regression sensitivity, not a production defect or a
  general mutation-coverage score. The mutated source is never published.

The suite is registered as `pmm-model` in `ms01/run.py` and associated with
SC-M1-024 through SC-M1-029. Board requirement/confirmation state is unchanged;
host evidence does not replace the held page-table-allocation guest supplement.

## Reproduce

```sh
sh krnl/tests/memory_model.sh
source efi/scripts/cloud-env.sh
export UBSAN_OPTIONS=halt_on_error=1
python3 ms01/build_coverage.py
python3 ms01/run.py --group host --output ms01/evidence/pmm-final-host
```

The model runner builds with ASan+UBSan and compiler warnings as errors. Eight
sequences pass with **225,293 public API calls** in total, followed by the
separate capacity/recovery process. The complete current build/host aggregate
contains **16 selected suites**. Consult its `report.json` for exact status;
selected host success is not a full MS-01 acceptance result.

Finite deterministic sequences do not prove every allocator state. The tests
do not validate guest page permissions, page contents, multiprocessor safety,
real large-memory machines, physical firmware or the unresolved noncanonical
branch. Existing runtime holds remain in place; no new physical-machine gate
is introduced and no source is pushed by this supplement.
