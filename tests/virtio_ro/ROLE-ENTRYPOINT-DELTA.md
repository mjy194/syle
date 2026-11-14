# Narrow real allocation-site API delta

Coordinated ownership: only linux_guest_memory.{c,h}, linux_vcpu_host.{c,h} and a
new guest_memory_role.h are changed. No firmware fvd/fmm/memory files, manager
selection, default CPU/#PF configuration or boot inputs are changed. Existing
uncommitted work was preserved; those four files were clean before this delta.

- `lgm_prepare_roles`: RAM/EPT purpose is selected at the real two allocation
  loops, before WB/map/zero. It invokes a role callback with owner.context. The
  legacy allocator may be NULL in this opt-in API; role callback may not be NULL.
- `lvh_open_roles`: VMXON and VMCS purpose is selected at the real control-page
  allocation sites, not by inspecting a PA or guessing allocation order.
- Original lgm_prepare/lvh_open delegate to the same internal code using the
  original allocate callback. No struct layout/positional initializer change.
  Existing quarantine, WB/map order, failures and release-token retention stay
  in the common implementation. No native manager selects the new APIs yet.

The callback still must bind role to a fresh actual allocation capability.
These enums are trusted call-site purpose, never ownership proof or guest input.
No mapping lease, release-during-VMX guard, device/vCPU exclusion or DMA admission
is created just by selecting a role.

## Tests run

Original LGM tests (all128MiB bytes, 18 failure boundaries, retained-release retry),
scattered backing/EPT walk tests and original VMX-host tests (128 open failure
positions, close retention) pass unchanged. New role-path host tests run actual
LGM/VMX-host source against actual optional PMM capability records with generic
allocate NULL. Full-capacity LGM uses verified real kernel/initramfs inputs; VMX
operations remain mocks. Type substitution, old-token/live-shadow, role-boundary
allocation/map/WB failure and wrong-control-type-before-map cases pass under
ASan/UBSan. No hardware VMX, guest boot or DMA test is claimed.

## Next lifecycle work, not falsely supplied by these APIs

A native-owned adapter must retain per-CREATE capabilities and map leases, and
must prevent owner release while VMX/EPT resources are active. A per-page pin alone
is not enough, especially since the current prototype allows only one lease per
page. Do not consume that one slot as both an implicit lifetime lease and a second
request lease without an explicit protocol.

The existing lvh_close sequence performs VMCLEAR, INVEPT and VMXOFF successfully
before setting active=false and releasing control pages. A future ownership-aware
lifecycle integration must obtain its release permission from that real sequence
and the controller's guest/device quiescence, not from a caller boolean. On any
failure retain records and forbid unsafe physical cleanup; if the sequence succeeds,
release and context drain can proceed with partial-cleanup semantics explicitly
tracked. LGM release comes after host close/platform disable in lgs_session.

Until that lifecycle wiring and mapping authority are complete, the new role APIs
are preparatory interfaces exercised in host tests, not an enabled native adapter.
