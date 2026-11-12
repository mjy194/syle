# N26: dedicated private-page UC admission

This is a single-CPU physical memory-type foundation, not a blanket admission
of all nested EPT12 types. The diagnostic executable never starts APs and
validates MADT contains exactly one enabled CPU matching BSP and rejects
additional disabled-but-online-capable CPUs. CPUID.1 package capacity is logged
only, not used as a machine-wide CPU count or admission gate. An irreversible AP-start-attempt marker
is checked; every INIT/SIPI startup path sets it before issuing INIT. It programs no
DMA device. IF=0, CR3 is the kernel-owned root, and PCID/PGE/LA57/CD/NW are off.
Only fresh allocator-owned low RAM data is eligible, never code, stack, table,
logging storage, firmware state or device memory.

## Audit and model

`cache_page_audit` walks every present leaf under the actual host root via a
bounded trusted table-reader callback. Shared subtrees are visited once per VA
path so aliases are not hidden. It refuses multiple/high virtual aliases,
huge-page coverage of the target, target used as any paging table, cycles,
unreadable tables, unsupported encodings and budget exhaustion. Exactly one
4KiB identity RW/NX mapping selecting PAT0 WB must remain; PAT3 must be UC.
This does **not** prove allocator ownership, DMA exclusion or other CPUs' CR3s.
Those are separate executable constraints, not inferred from PTE permissions.

The registry closes access during transitions and records a strict ticketed
sequence. Its acknowledgements are software bookkeeping, not a substitute for
executing the real instructions. Tests cover stale/out-of-order acknowledgments,
overflow, poison, and host alias/huge/table/PAT/permission failures.

## Actual transition

The only guest EPT root is private to this diagnostic. Its complete PT is
checked for exactly one alias of the target and upper levels are verified.
No VMCS is live during transitions. The mapping is removed and a successful
single-context INVEPT occurs before host type changes.

1. Remove old host PTE; INVLPG its only virtual address.
2. WBINVD, while the target is nonpresent and cannot be accessed.
3. Install new host PTE with the selected pre-existing PAT index; INVLPG.
4. WBINVD again before permitting any target access or guest mapping.

Only then install EPT UC/RW/IPAT and INVEPT. The dedicated guest reads the old
value, writes 0x77 and reads it back. Host reads the same UC mapping to verify.
Other host/guest pages and all EPT tables stay WB. Guest PAT is ignored only for
this UC data leaf, so no WB/UC alias is created for the physical data page.

After VMCLEAR, detach EPT and INVEPT; perform the same break-before-make protocol
to restore WB. Verify original host PTE identity (hardware A/D changes allowed)
and data, then VMXOFF and free. Every uncertain failure poisons and retains;
no speculative rollback or freeing partially transitioned memory is allowed.
PAT/MTRR MSRs and CR0.CD are never changed.

Basis: Intel SDM Vol3A §11.12.4 (new editions §12.12.4), example for remapping a
page with a different type: remove cacheable PTE, flush relevant TLBs, establish
new mapping and flush caches. This implementation additionally flushes caches
while detached and never relies on self-snoop exceptions. This is distinct from
the multi-processor protocol for changing PAT/MTRR registers, which we do not do.
Official source: https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf

`make cache-uc`, `tests/cache-uc.sh`; real UC execution requires isolated KVM
`-smp 1`. Cloud TCG only verifies capability rejection. Multiple CPUs, WC/WT/WP,
DMA, device mappings and integrating this lease into arbitrary nested contexts
remain unsupported and require their own rendezvous/ownership/cache contracts.

Before any EPT detach/INVEPT, primary secondary-control support, secondary EPT,
and EPT walk4/WB/INVEPT/single-context capabilities are explicitly gated.
Dependent MSRs are read only when their preceding capability is present.

## N26 hardware evidence (2026-11-09)

Final kernel SHA-256:
`65686b74eb3457c9ebbfbf0095e3ae61182765594766ae83bc863c4180edcbf0`.
Archbook isolated QEMU/KVM, `-cpu host -smp 1`, 256 and 512 MiB:
both exit 33 with `CACHE.UC_GUEST_READ_WRITE_PASS` and `CACHE.N26_PASS`.
The logs confirm validated single-CPU MADT/no AP start, one audited host alias,
host PAT3 UC plus EPT UC/IPAT, two break-before-make transitions, and WB
restoration before freeing. Target HPAs were 0x191000 and 0x211000 respectively.
No test VM remains running. Evidence Library ID:
`libfile_fa21cab8bd908191bf6b7128fabf77fc`.

An earlier candidate was safely blocked by a CPUID package-capacity check;
package capacity is not the active VM CPU count. Final admission instead uses
validated MADT enabled CPU count and BSP identity, rejects online-capable
inactive CPUs, and rejects any AP-start attempt. CPUID capacity is informational.
The blocked candidate did not execute a UC conversion. This result establishes
the dedicated single-CPU UC lease only, not arbitrary nested non-WB mappings,
SMP cache-type transitions, device/DMA safety or complete memory-type support.

Final independent regression: `efi/build/cache-uc-topology-fixed-review-full.log`,
`FINAL_STATUS=0`. Includes admission/alias/transition negative tests, N18–25,
nested N0–17 and previous non-IOAPIC regressions. Cloud capability skips are
not counted as hardware VMX passes; the two KVM UC runs above supply that proof.

## N27: registered UC through real nested EPT

`cache_registered.c` is a one-lease, one-composed-context service. It repeats
N26's real MADT/no-AP and capability gates before allowing any transition.
A dedicated allocator-owned L1 GPA f000 backing page is used, not L2's stack
page8000. EPT01 initially detaches its only alias and flushes; host transitions
to UC; EPT01 is then installed UC/IPAT/RW. L1 can therefore access the same data
without a WB alias. All EPT table pages, code and stacks remain WB.

The typed resolver provides UC data only for the exact registered HPA while
phase=UC and the pinned lease epoch matches. It returns no table pointer for
that data page, so using it as an EPT12 table fails closed. Ordinary WB resolver
and callbacks cannot accept this UC target as WB. EPT12 UC/IPAT1 can map this
backing; WB/IPAT1 can map only verified ordinary owned WB pages. A legal but
unsafe type pairing stops, not a fabricated EPT misconfiguration. Walk type
remains WB; EPTP UC, A/D tracking on UC, other non-WB types and dynamic type
conversion during guest execution are not supported.

Sparse typed admission is explicit and defaults off. It runs on every new fill
and cached lookup/permission operation, checks the current lease epoch/phase,
and encodes/verifies actual leaf memory type plus IPAT, rather than hardcoding
WB. One tree-wide pin is acquired before initialization/publication. INVEPT
reset removes leaves but retains the lease pin. OOM or flush failures retain
it until successful tree destruction; there is no premature target reuse.

L1 installs two L2 GPAs e000/f000 pointing to UC L1f000. Two GVA aliases select
PAT WB and UC, but both are UC because IPAT1 applies at both EPT layers. L2
cross-writes/reads 0x77; L1 independently reads the UC data, issues INVEPT,
then resumes L2 to cross-write/read 0x99. No decision is based on one observed
GVA's PAT. The CPU's actual guest PAT is checked at each virtual exit.

Cleanup clears VMCS02, destroys/flushes all its sparse translations, clears
VMCS01, then releases the tree pin. EPT01 target is detached and invalidated
before the host break-before-make transition back to WB. Only then may the
outer fixture release pages. Failed clear/flush/transition halts and retains.
This is single CPU and a single explicit composed context, not general
multi-context type migration, multiprocessor cache management or device DMA.
