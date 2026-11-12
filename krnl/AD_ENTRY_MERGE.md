# Single-entry A/D merge model (no runtime integration)

`ad_entry_merge.c` is an independently tested algorithm with an injected
backend. It does not replace `nested_ept_ad_update`, emit atomic instructions,
resolve guest addresses or enable concurrent EPT12 walks. Existing runtime
paths retain their caller-serialized contract. No frozen N32B image is rebuilt.

The caller supplies an expected non-A/D identity, a mutable mask (A for a
nonleaf, A|D for a leaf), this operation's requested bits and a stamp binding
domain ID, domain incarnation, context ID, context epoch and table lifetime.
A caller-validated failed-walk prefix may request A only. A nonpresent terminal
must never be submitted. Present/format/permission checks and whether a dirty
update is justified are caller responsibilities, not inferred by this model.

The backend provides a linearizable atomic load/CAS and a stamp/ownership
admission check. A lifetime pin must hold the aligned, writable WB entry alive
for the entire call, **including after guard failure**. Checking a stamp after
access does not prevent use-after-free. A real backend would require supported
lock-free 64-bit atomics and complete owned-page validation. This test backend
is not proof that such a backend or a multi-CPU lifetime protocol exists.

Each retry validates admission, loads the entry, rejects any non-A/D identity
change, and CASes `current | requested_bits`. CAS contention may be retried
within a finite budget. A previous trace's remembered D is not restored.
Competing A/D changes are preserved; an identity change requests a fresh walk.
No-op success means the bits were observed set at one instant, not that another
writer cannot subsequently clear them.

Results distinguish invalid input, completed/no-op, identity rewalk, stale
stamp, bounded retry exhaustion and **applied-but-stale**. `changed` reports a
successful value-changing CAS by this invocation, not mutations by competitors.
A successful CAS followed by failed admission requires upper-layer retention
and recovery; it must not be treated as an ordinary safe retry or rolled back.
There is deliberately no guarantee that every failure has zero side effects.

## What CAS cannot establish

* A consistent four-level walk: ancestor identity may change between distinct
  entry updates. Several individually atomic operations are not a transaction.
* ABA/lifetime safety: a table may be removed and reused with identical bits.
  An externally held pin and non-reused lifetime identity are required.
* Synchronization with clear-A/D plus INVEPT: checking an epoch before CAS is
  not exclusion. An old updater can write after a new epoch unless it is
  drained under a real entry/publication gate. Post-check detection only reports
  the race. Old cache promotion or refill must not be published after reset.
* Exclusion of ordinary guest stores: a host lock or host-only version counter
  cannot stop L1/L2 writes through another alias. All writers must be stopped,
  their aliases write-protected/intercepted, or participate in a real protocol.
* Atomic failed-walk effects: successfully set prefix A bits cannot be undone
  without potentially erasing other CPUs' updates. A future path API must
  report partial effects, not retain the old `false means no writes` contract.

Tests use deterministic injected interference: another A/D writer, remapping,
permission/type changes, stamp changes before/after CAS and after no-op,
spurious CAS failures, cross-domain/context stamps, invalid masks and dirty
requests. They validate this algorithm's explicit assumptions, not hardware
EPT A/D ordering or full nested multi-vCPU correctness.

Run without touching EFI build artifacts:

```
cc -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  krnl/tests/ad_entry_merge.c krnl/ad_entry_merge.c -o /tmp/ad-entry-merge-test
ASAN_OPTIONS=detect_leaks=0 /tmp/ad-entry-merge-test
```

Independent review (2026-11-09): `/tmp/ad-entry-independent-review.log`,
-O2/-Werror + ASan/UBSan passed. This is injected-backend pure-algorithm proof,
not a real concurrent nested runtime result. No full build or hardware path was
changed. Frozen N32B kernel remains
`35664b81d2906fdf740dc7ae94e7ab5138293c83462f517e6a15e930faca7c2c`.
