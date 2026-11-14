# Source permissions: one real stopped-guest observation

## Identity and evidence

The isolated READY mode0 kernel SHA256 is
`ac33eaf3070868273679fcb765e66e602f51842a76635723bd9120e741cb245f`.
This was the previously frozen baseline probe, NOT the new managed bounded ELF.
The reviewed v2 control manifest SHA256 was
`e04cd39969ff042f0c919a1b4ff9e339fecc61659ab924b91a1b0212204170c3`.
Raw evidence archive SHA256:
`23e67b15c576fdb18d94c1940fd801212a18a4083ff8d6fea10415ad105e4c9e`
(10,012,537 bytes). Independent replay is retained at
`evidence/source-v2-independent-replay.json`; the offline tool is
`verify_source_capture.py`.

The prior v1 attempt was INCONCLUSIVE before memory snapshots because the
FlatView parser did not recognize QEMU's exact KVM suffix. It provided no source
permission result. The corrected parser retained its conservative RAM whitelist;
it did not bypass the refusal or silently widen the address range.

## Independently replayed result

The raw QMP trace, physical bytes and session snapshot were independently read,
not just the runner summary. One cont reached the READY wait; STOP captured
CPL3 CS=0x2b, RIP=0x8000000054, SMM=0. No inject-nmi occurred. All 840 pmemsave
requests satisfied the stopped system FlatView RAM whitelist and unique output
checks: 834 cached physical pages plus 6 fixed state snapshots. The process
acknowledged quit, exited 0 and was reaped (12.253 seconds recorded runtime).

Captured registers:
- CR0 = 0x80010033 (including supervisor write protection).
- CR3 = 0x4436000 (Syle's current management/CPL3 root).
- CR4 = 0x2668; EFER = 0xd00.
- PMM conservative address bound = 2^46, derived from its CPUID limit with a
  possible downward clamp; not an assertion of exact CPU MAXPHYADDR.

Replay traversed 265 table pages and recomputed exact source bytes, checking
actual session source pointer/length identity against the frozen ELF inventory:

| Source | Bytes | SHA256 | Reachable overlapping leaf mappings | Writable observed |
|---|---:|---|---:|---|
| kernel | 984064 | 3d1c081c18e64ad97a44e2b038b8b3d0c733d38494b6293ba2402898c3a629e6 | 241 | no |
| initrd | 1332736 | 3a3d60206c77881f87f5c10fc5433d09b8302e134058bc10a40f871fd4fdcf39 | 327 | no |
| command, including NUL | 74 | f6b42c8a1eb3cfbc080174267b380ee8288b0c47943d94a72cc638f41d0460c4 | 1 | no |

The source translations were effective supervisor, read-only and NX under the
captured hierarchy. Counts above are leaf mappings overlapping the byte ranges,
not a count of additional aliases. All exact hashes and per-leaf reports matched
the independent replay. The grammar, page-table and permission logic already
had separate host review; replaying the raw capture does not create a second
independent implementation of that parser.

## What this does NOT establish

This is one stopped snapshot of current Syle CR3 guest-virtual -> QEMU guest-
physical translation. It is not nested Linux EPT, Linux's own CR3, host physical
translation, another CR3, or a proof against device DMA. It does not prove
long-term source immutability, page-table lifetime, absence of arbitrary kernel
writes, complete NMI safety or safety of enabling native scoped mapping.

In particular, actual linux_guest_manager.prepare calls mhc_enter with
paging_root() before lgs_prepare. That kernel root differs conceptually from the
observed CPL3 root. ring3.c inherits lower/high kernel subtree entries, but a
static sharing relationship is not a captured runtime proof of both roots.
The next narrow passive comparison can inspect the exact paging-root data symbol
and that root's reachable source mappings at a stopped boundary. This must not
be conflated with the current result, and would still be a snapshot, not lifetime
ownership. Changes to source data/mapping or actual callback-context observation
need separate review and root coordination.

No native bounded adapter was activated by this experiment. Native refusal and
the existing native-map panic remain intact; no guest DMA is authorized by this
observation. Actual VMX open/close and source/controller lifetime remain separate
activation prerequisites.
