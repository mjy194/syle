# Next native observations: small, passive, no bounded-map activation

## Object/state graph to preserve

Controller CREATE owns one binding -> one actual session -> one source owner
(private kernel image/initrd/cmd backing) and one allocator epoch. That epoch
owns generation-bearing RAM/EPT/VMX control capabilities. PREPARING has a source
reference before first inspect. Each destination operation borrows its explicit
allocation under that fixed controller/epoch; source admission and scratch copy
are inside the borrow. READY cannot be claimed until hardware-lifetime ownership
and real VMX prerequisites exist. STOP is not teardown. On failure, revoke new
operations; known completed copies may release short pins; partial owners stay
recorded. Real host close must complete before RAM/EPT owner release, allocator
end and final source release. Fatal/unknown completion retains everything and
must not return into normal REAP cleanup.

Current managed host tests cover this with MOCKED host open/close and no START.
The static native adapter unconditionally refuses memory/host-open. It has not
booted. Do not treat symbol linkage or a matching source pointer as READY.

## Actual source route, not a hypothetical user-buffer API

linux_guest_manager.c currently configures linux_kernel_start/end and
linux_initrd_start/end, plus a constant command string. managed-baseline.mk and
managed-guest.mk place embedded assets in `.rodata,"a"`. krnl/link.ld gives the
rodata PT_LOAD R-only flags. paging_init later protects __rodata_start/end with
NX and no RW, sets CR0.WP, and installs its root. For low PA (<4 GiB),
paging_phys_to_virt uses identity mapping; high PA uses a distinct direct window.

Those source facts suggest a narrower first native source profile: ONLY the
compiled embedded assets and fixed command literal in the measured kernel's
rodata, never arbitrary external pointers. ELF flags alone do not prove current
PTE permissions, CR0.WP, lack of other writable aliases, stable page-table
ownership or that any other device cannot write physical memory.

## Step A: read-only stopped-guest source observation (next runnable candidate)

Use a fresh isolated guest and existing trusted launch/identity/cleanup discipline,
serialized by the inputs runner. No host NMI, no QMP inject-nmi, no device
passthrough/NIC and no writes to guest RAM. Prefer an existing pre-entry manager
wait boundary, not an added arbitrary kernel breakpoint. Stop with acknowledged
STOP event and running=false; capture registers/CR3/CR0/CR4 and exact ELF symbol
ranges. Read guest physical page tables via its private QMP connection.

Validate every embedded source page's translation, effective supervisor/RW/NX,
physical range and captured CR0.WP. Read hash of its bytes against the frozen
artifact. A bounded walk of reachable page tables can additionally report any
writable mappings to those SAME source physical pages; treat unsupported levels,
reserved bits, excessive node budget, ambiguous repeated tables or missing
memory as inconclusive/fail-closed. Page-table readers need independent host
parser tests before a guest run. Do not change page permissions to make a test
pass. End with clean guest quit and retain raw evidence.

This establishes one stopped snapshot's observed source mapping, not persistent
immutability, absence of DMA, hidden segment state or arbitrary kernel writes.
If source protection is absent, report the blocker without enabling bounded copy.
This is more useful than rerunning the already-completed two CPL3 NMI samples.

## Step B: actual VMX lifecycle observations are separate

Actual lvh open/close host instruction mocks are not hardware proof. Reuse an
independent tiny guest with known deterministic VM exit, existing actual native
VMX calls and owned pages, but do NOT couple it to bounded DMA. Observe admitted
host CS/SS/TR state and exact VMCLEAR/INVEPT/VMXOFF completion at real return
boundaries. A bounded isolated assembly observation hook may be needed; that
requires root file ownership coordination and review before building or running.
Known success can be observed; uncertain instruction completion is fatal and
must not be injected as a recoverable false result. Any release-order experiment
must retain pages until genuine quiescence, not rely on timestamps or mock flags.

## Step C: activation remains gated

Only after measured-source/alias policy, stable mappings, actual VMX close,
controller identity and asynchronous/fatal rules are separately validated should
a tiny bounded native copy be proposed. First target would be a new private
unpublished allocator page, never a live guest queue or disk backing. This step
is NOT authorized by passing A or B alone and is not implemented here. Native
memory refusal and existing native-map panic stay in place throughout A/B.
