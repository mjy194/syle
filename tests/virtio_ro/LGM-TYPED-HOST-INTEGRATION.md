# Real LGM / actual PMM typed callback integration test

`lgm_typed_host.sh` now executes the repository's actual lgm_prepare, lgm_release
and Linux loader against the actual optional PMM authority with 32,837 records.
The input hash checker validates the real baseline bzImage/initramfs first.
This is **host execution only**: the CPU mapping callback returns aligned malloc-
backed test storage, the guard is mocked, no VMX instruction/guest/DMA runs, and
no callback from this test is installed in native code.

## What is connected

The sg_memory_ops allocation callback uses fresh authority typed allocation and
stores its exact generation capability. Map/WB/release resolve a record, then
query the authority for current epoch, PA and immutable type. They do not trust a
bare owner label or reconstruct a capability from a PA. The mock uses a bounded
contiguous physical-address fixture solely to index host backing; this is not a
native GPA/HPA mapper and not a paging_phys_to_virt substitute.

The additive real `lgm_prepare_roles` API now supplies SG_MEMORY_RAM and
SG_MEMORY_EPT explicitly at its allocation sites. The host callback translates
those roles to fresh authority typed allocation; it no longer infers purpose from
allocation ordinal. Counters only assert the known sequence and bounds. It then
seals EVERY actual m.pages[] / m.tables[] entry against authority kind/generation.
Both EPT-ledger substitution and context-record type tampering are negative cases.
The original `lgm_prepare` compatibility API and default manager caller are unchanged.
Native manager binding and mapping/lifecycle proof are still separate work.

After real LGM preparation, two additional allocations request VMX_CONTROL and
are verified as such. They exercise capacity/type accounting for VMXON/VMCS, not
actual lvh_open or VMX hardware. The full 32,837-page fixture is then exhausted;
an ordinary PMM allocation correctly fails rather than returning a protected page.

## Verified failure and lifecycle cases

- Real baseline kernel/initramfs loading succeeds; copied initramfs bytes match.
- All RAM/EPT ledger entries seal against authority-held capabilities/roles.
- Actual LGM release with injected no-effect failure on one RAM page keeps that
  exact LGM token and authority record while successfully releasing the other
  RAM/EPT pages. Two independent VMX_CONTROL records remain accounted too.
- Session end rejects retained records. Drain, release retry and explicit control-
  page cleanup bring PMM allocation count to zero, then end succeeds.
- Next epoch reuses the first PA, while prior-context mapping/release callbacks
  reject it. New epoch's capability remains independently releasable.
- Separate processes inject allocation, WB and map failure at the first EPT page
  (the real RAM→EPT boundary). Actual lgm_prepare unwinds all owned pages and
  returns unprepared with zero PMM allocations; session drain/end succeeds.

All run under strict warnings plus ASan/UBSan. The map/writeback injections and
release failure are host mocks with documented contracts; they do not show native
cacheability validation or prove arbitrary partial DMA-write recovery. A release
failure is no-effect, unlike a possibly partial store.

## Still missing before native use

This test's map API has no retained mapping-lease handle. Returning a host mock
pointer under sequential test control cannot prove a real native mapping stays
valid through VM execution, IO commit or teardown. There is no global alias proof,
no device/NMI/AP exclusion and no hardware publication test. The additive real allocation-site APIs are present, but native manager binding,
role-aware context ownership, lifecycle evidence and mapping leases
must be implemented and tested before wiring DMA or guest entry. The final ELF
proof still substitutes PMM only; these typed callbacks are NOT in that image.

## Stronger permanent evidence

Context type-label tampering is rejected by actual authority queries. After a
new epoch reuses a PA, the old record is deliberately marked live again while
retaining its old generation; old map/release still fail and the new allocation
remains queryable/releasable. This avoids attributing stale protection merely to
an already-cleared live flag.

`vmx_role_host.sh` separately executes actual `lvh_open_roles` with actual PMM
capabilities and mock VMX operations. Real source allocation sites supply distinct
SG_MEMORY_VMXON / SG_MEMORY_VMCS roles; generic allocate is NULL. All 128 open
failure positions and clear/INVEPT/VMXOFF/free-retention cases run, with PMM count
zero after cleanup. Wrong authority kind is rejected before mock mapping. These
are not hardware VMX calls and do not install a native adapter.
