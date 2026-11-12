# N40: controlled immutable EPT12 catalog and owned backing

N40A is a pure production substrate. N40B is its explicit opt-in runtime
adapter (`make -C efi smp-nested-root-catalog`). The previously frozen N39B2
kernel/evidence remain unchanged and are not evidence for this new kernel.
There is no arbitrary L1 structural writer, source replacement, shared
multi-root A/D, or new backing-reclamation permission in this stage.

## Authority and identity

`nested_root_catalog` owns bounded caller-provided root/span arrays. Trusted
host provisioning registers records transactionally and publishes them once.
Registered storage belongs exclusively to the catalog immediately, including
before publication. No open-generation edit, reopen or replacement API exists.
A new source lifetime requires a new, never-reused catalog identity and prior
teardown of every old software/hardware cache reference. Array capacity, nonce
exhaustion, owner pool size and retirement receipt capacity are explicit limits.

Each root binds domain/incarnation, catalog lifetime, source ID, owner,
structural generation, source lifetime, full EPTP and separately decoded root
GPA. Exact full keys are compared; changing EPTP configuration cannot select
the old root. Initial support is four-level WB/AD0 EPTP and 4K leaves. Structural
source generation is distinct from typed-lease epoch and EPT02 cache generation.

Backing spans bind owned object/lifetime, GPA and HPA ranges, permissions,
allowed cache types, IPAT and TABLE versus DATA role. Every range, end,
alignment and physical-width limit is checked. All GPA overlaps and HPA aliases
are conservatively rejected. Writable or non-WB table spans are rejected.
Only catalog-designated read-only TABLE spans expose a host table view; ordinary
WB DATA and the dedicated typed page never become table bytes merely because
a guest entry points to them. Guest addresses are never treated as host HPAs.

The trusted authority callback must prove current allocation identity/lifetime,
all guest aliases and actual EPT01 permission/type for the entire page. Its
returned object, lifetime, HPA, view, permissions and cache attributes are
matched against the immutable record. This callback is an explicit external
ownership proof obligation, not a fact inferred from a pointer or Boolean.
The runtime constructs its records only after the existing complete unique
allocation and all-alias fixture audit, retains them through final teardown,
and checks the actual owner's EPT01 path and leaf at every resolution.

## Whole-walk lifetime and faults

Stable caller-owned sessions use a bounded metadata lock, intrusive live list
and saved nonce. A close prevents new sessions while old admitted sessions may
finish. No lock spans a resolver, walk, allocation or invalidation. Failed
release preserves the live session and backing requirement; storage cannot be
stack-discarded. Lock exhaustion poisons admission. Closed/drained alone never
proves that physical/cached consumers are gone or authorizes page release.

A successful walk validates complete L0 backing, actual permissions, leaf type
and IPAT before deciding a complete-path L1 permission fault. A nonpresent
terminal remains an L1 fault and its ignored payload is not resolved.
Unsupported formats remain unsupported, never invented reason 49. Final source
identity is rechecked before returning a usable translation. Immutable structure
is still a precondition: the API does not claim to detect arbitrary raw stores
or structural ABA outside its contract.

The new `typed_root_registry_miss_source` is opt-in. Its existing directory
operation spans initial source guard, cache lookup, translation, pre-fill guard,
fill and post-fill guard. The legacy miss API is unchanged. A late guard failure
after publication poisons and retains the tree. L1/L0/unsupported translation
status remains available to the adapter. No persistent external refill plan
escapes a call; lifetime sessions are stored in the stable owner object.

## N40B bounded runtime

The new target retains N39B2's owner-private registries/pools, budget two,
controlled overlap and root-scoped completed INVEPT evidence. Diagnostic
provisioning still builds three roots and 63 owned pages. Those constants are
fixture data, not the reusable catalog/owner admission or resolver API.
Profile admission and initial selection use the catalog, not the old A/B/C
whitelist. The owner binds one immutable catalog pointer and identity for its
entire cache lifetime. Entry checks the complete selected source token even
on a warm hit; changing catalog or source identity cannot reuse old cache state.

The actual A/B/C guest sequence accesses both the typed data range and the
separate root-dependent witness range, with WB→UC→WB transitions and B2 overlap.
Per-owner diagnostic probes call production code to reject a foreign domain,
stale source generation, unowned GPA and denied permission. Their L2 and L1
witness addresses are explicitly supplied by fixture provisioning; generic
binding does not assume any L1 table address is identity-mapped in L2.

Type transitions retain the established actual-root checkpoints, source
revocation, cache reset, directory close/seal, host break-before-make, cache/TLB
maintenance, exact ACKs and reopen-last protocol. Before source leaf retype,
both stable source sessions must be inactive and a synchronized catalog idle
snapshot must confirm zero leases. The catalog remains structurally immutable;
only the established controlled leaf type changes at a distinct typed epoch.
Final catalog close/drain follows both consumers' actual CLEAR/OFF and AP park,
not merely a local L1 INVEPT or a cache-root retirement receipt.

## Verification and remaining evidence

- `efi/tests/nested40a.sh`: actual catalog plus actual typed registry/pool tests.
- `efi/tests/nested40a_independent_review.py`: independent boundary tests,
  15 negative mutation witnesses and 100 eight-thread admission/close schedules.
  The concurrent suite has also passed a separate GCC TSan run.
- `efi/tests/nested40b.sh`: actual adapter with trusted host physical-operation
  hooks, WB/UC eviction/recreation, negative probes, warm source checks, arbitrary
  catalog profile keys, old profile regressions and compiled completion slice.
  `nested40b_independent_review.py` also exercises the unchanged production
  entry/retype/final-release gates and ten additional mutation witnesses.
- Runtime backend checks and exact-ESP KVM are separate from host tests. TCG's
  missing-VMX capability skip is not a runtime PASS.

Independent pure and runtime-host review passed after correcting leaf cache
attributes/fault precedence, moving environment validation outside the metadata
lock, and removing an implicit L1-table/L2 identity-map assumption from binding.
The focused build and TCG capability skip passed. Final freeze, fresh full
regression and isolated same-hash KVM evidence remain separate requirements
before claiming N40B hardware acceptance.
