# N46B staged physical bootstrap proposal

This follows reviewed N46A, not the AD0 live-transition path. No source changes
or host cache changes are made by this plan document.

1. Before AP start, validate the complete original owned WB fixture and unique
   allocations. Bind an unpublished fixed-UC catalog/lease policy and retain all
   source/data backing irrevocably. No source lease, updater or guest can enter.
2. Audit private pool and VMCS allocations as distinct WB storage. All five fixed
   maps must be from the constructor's exact full-tree shape. The call sequence
   starts before either VMCS guest entry; verify both owner VMCS/registry
   states. Both CPUs then enter VMX root while guest admission remains closed.
   A zero history counter alone is not proof of nonresidency, so every fixed
   physical EPTP is explicitly invalidated on both owners in steps 4 and 5.
   There are no guest/DMA/other-VM consumers of this RAM.
3. Start both CPUs with interrupts disabled and guest admission still closed.
   Each independently proves original host WB mapping, MTRR WB, stable CR0/CR4/
   CR3, legal existing PAT with PAT0 WB/PAT3 UC, and a full unique RW/NX host
   alias. Compare the snapshots. No PAT/MTRR register change is permitted.
4. Detach the target from every fixed EPT root while guest entry is closed.
   Both owners execute actual INVEPT for all five fixed EPTPs and publish
   distinct exact receipts. Source metadata remains unpublished. Break the sole
   host PTE,
   then require separately attributed INVLPG and WBINVD receipts from both CPUs.
   No dereference, checksum or data audit may touch the absent target.
5. Install the UC host PTE using the exact retained host-PTE identity, then
   require both CPUs' post-make INVLPG and WBINVD receipts. Audit current UC on
   both CPUs. Install final fixed UC EPT aliases/source words, then execute
   actual INVEPT for all five fixed EPTPs on both CPUs again. Final phase-aware
   host/alias audits and exact receipts precede publishing the source catalog
   once. WB source tables and all non-target data
   remain unchanged in type. A failure poisons admission and retains RAM.
6. Create owner-private AD1 caches normally. Their initial INVEPT evidence is
   separate from host provisioning receipts. Both VMCS01/02 entry paths enforce
   guest CD/NW masks and all-MSR interception; current owner PAT/CR0 policy is
   checked on every admission. Guest PAT is ignored for IPAT1 mappings.
7. Reuse the bounded two-reset/13-checkpoint guest schedule with UC A/B target
   and WB alternate C target, checking physical UC flags, independent per-owner
   dirty tracking, two A/D epochs, fixed type epoch, exact reset/retire receipts
   and retained data. No live retype, source mutation or UC huge leaf is added.
8. After actual VMCS clear, cache retirement, VMXOFF and AP park, keep UC host
   mapping and all backing retained. The existing atomic backing-release guard
   must remain effective even on an interrupted bootstrap. No reverse WB
   transition or backing-free result is claimed.

Tests must mock and exercise the actual staged memory adapter and provisioning
control order, including alias omission, foreign allocation, missing/duplicate/
wrong-CPU receipts, wrong host-PTE identity, source premature publication,
nonpresent-window target access and all partial-failure retention paths. Build
and physical execution acceptance remain separate, parent-coordinated gates.
