# Bootstrap physical page allocator

`memory.c` provides the single-CPU, interrupt-disabled physical allocator used by
initial kernel paging. It never calls EFI and does not zero or dereference a
returned physical page; the paging caller must establish/retain identity access
and initialize the contents.

## Admission and ownership

- Only complete 4 KiB pages in EFI ConventionalMemory (type 7) below the minimum of CPUID MAXPHYADDR and 2^47
  with EFI_MEMORY_WB capability (attribute value 8) are candidates. Runtime, read-protected, write-protected and read-only ranges
  are excluded. Low 1 MiB is always reserved.
- All other EFI descriptor types stay reserved, including BootServicesCode/Data,
  LoaderCode/Data, ACPI, runtime services and MMIO. No firmware memory reclamation.
- EFI image, kernel allocation, user ELF bytes and full allocation, map buffer,
  boot_info and framebuffer are explicitly reserved, rounding out to pages.
- Entire conventional descriptors containing inherited RSP, inherited CR3 root
  or the ACPI RSDP are excluded because those opaque objects have no complete
  ownership/size description. This is conservative, not a general table walker.
- Sorted sparse eligible/free extents replace the old 4 GiB bitmaps. Eligibility
  minus free pages is allocated ownership. Free extents coalesce; at most 4096
  extents are stored. Fragmentation capacity exhaustion returns failure without
  changing ownership (including a failed free, which the caller must handle).
  Physical address width is queried from CPUID; RAM past that limit is rejected.
  RAM above the 128 TiB direct-map design limit is excluded, not counted usable.
- Reinitialization is refused after successful initialization, so active ownership
  cannot be accidentally forgotten. Empty usable memory is a valid initialization;
  allocation then returns zero.

## Input contract and bounds

The trusted loader must supply readable boot_info/map backing memory; validating
an arbitrary physical pointer's readability is outside this parser. The parser
checks boot ABI, EBS and no-reclaim state, buffer capacity, EFI descriptor version
1, stride (at least 40 bytes and 8-byte aligned), map divisibility, at most 1024
descriptors, physical alignment, nonzero page counts, arithmetic overflow and
pairwise non-overlap. It accepts unsorted descriptors, ignoring bytes after the
standard descriptor prefix. No allocator state becomes live until every descriptor
and reservation range passes validation. A failed initial attempt remains closed.

`pmm_alloc_page()` returns a physical address or zero; `pmm_free_page()` returns
1 on success; `pmm_get_stats()` gives usable/free/allocated page counts. There is
no concurrency, contiguous allocation, NUMA, hotplug, DMA constraint selection,
page content scrubbing yet.

## Host verification

Run `krnl/tests/memory.sh`. ASan/UBSan cover bad metadata/strides, capacity and
arithmetic overflow, unaligned/empty/overlapping descriptors, explicit reservations,
firmware, runtime and UC-only exclusion, low-memory exclusion, upper-address clipping,
exhaustion, uniqueness, free/reuse, double free, unaligned/reserved/out-of-range free,
and reinitialization ownership preservation. Leak detection is disabled for this
sandbox's ptrace limitation; the test itself uses no dynamic allocations.

## Early page-table integration

`paging.c` consumes pages from PMM for every page-table level, zeros them, and
keeps them allocated for the lifetime of this bootstrap address space. Pages
are not freed while installed. Tables identity-map RAM below 4 GiB using 4 KiB leaves. Higher RAM uses
`0xffff800000000000 + physical` with 2 MiB leaves inside each descriptor and
4 KiB edges; it is supervisor-only and NX. Mapping does not transfer ownership.
`paging_phys_to_virt` converts admitted RAM physical addresses after the switch;
it does not validate an arbitrary address is mapped or allocate a mapping.
Page tables themselves are allocated below 4 GiB so bootstrap firmware access
and the existing table walker remain safe. Running out of low table pages fails
explicitly, even if high RAM remains. Kernel
text/rodata/data override general NX RAM permissions, and GOP framebuffer uses
the verified PAT UC slot. CR0.WP and EFER.NXE enforce permissions in supervisor
mode. RAM mappings are supervisor-only and inherited by the Ring 3 root.

The first switch occurs after own GDT/IDT/TSS initialization, before smoke/fault
or management paths diverge. Constructing tables still assumes firmware identity
access to free RAM. Firmware allocations remain reserved after switching. This
is not a complete VM subsystem: no demand paging, mapping teardown, TLB shootdown,
heap or SMP. User code is loaded through a kernel-writable/NX backing alias,
then `paging_seal_code_page` removes write permission from every leaf covering
that physical page in the owned root and flushes the BSP TLB before the user RX
alias is published. The user root inherits the sealed supervisor tree. The same
seal precedes executable trampoline publication. Initial kernel text has only
its RX identity mapping; low physical RAM is not also mapped in the high half.

The seal walks aliases by physical address, not just a primary virtual address.
It splits intersecting 1 GiB/2 MiB leaves down to 4 KiB while preserving neighbor
permissions and PAT/cache flags. It requires the current owned root, interrupts
disabled, PGE/PCID/LA57 off, and no attempted AP start. Allocation failure panics
before new execution is published. It is intentionally irreversible: callers
must not re-create writable aliases or recycle sealed code. This is not a JIT
permission-toggle API, multi-root process manager, EPT policy, or AP shootdown.
The current bootstrap owns its page tables; arbitrary external table mutation
is outside this guarantee.

`krnl/tests/paging_alias.sh` runs the production seal against synthetic tables
with low/high/user aliases, 1 GiB and 2 MiB splits, PAT preservation, neighboring
pages, rejected contexts, and address conversion boundaries. Its flush is a host
hook, not hardware evidence. `efi/tests/wx_alias.py` separately requires real
QEMU #PF error 3 and exact CR2 when supervisor code attempts writes via the
previously writable kernel alias and the published user RX alias. Code loading
warms the old writable translation before sealing. `efi/tests/ring3.sh` checks
that executable code still runs and ordinary user isolation is retained.

Cacheability: allocator pages must advertise EFI_MEMORY_WB. General RAM mappings
choose WB when supported, otherwise UC when supported; unsupported cache policies
and read-protected descriptors are left unmapped. Required boot ranges must already
have an admitted RAM mapping or boot stops. Permission refinements preserve cache
bits. PAT entries 0=WB and 3=UC are checked, not overwritten. This does not implement
a general cache-attribute manager or arbitrary platform MTRR/PAT reconciliation.

## High-memory verification and remaining placement constraints

`efi/tests/high_memory.py` boots actual 256 MiB, 5 GiB and 8 GiB QEMU/OVMF
configurations. The kernel allocates a physical frame above 4 GiB, writes and
reads all 4096 bytes through the new direct map, then releases it. The Ring 3
root inherits high-half supervisor mappings and the kernel probes them again
before entering user mode. A 5 GiB diagnostic also attempts a user-mode read of
a known-present high supervisor mapping and requires #PF error 5. These tests do not establish arbitrary huge-machine
support or native hardware correctness.

The allocator unit suite also covers sparse 1 TiB/32 TiB ranges, CPU bounds,
canonical-cap clipping, and free-extent exhaustion/recovery. Actual bootstrap
objects (EFI image, boot info/map, ELF buffer, fixed-address kernel and GOP)
still must be below 4 GiB; this is checked before CR3 replacement. This is not
a limit on managed RAM. The loader has not been generalized for high-placement
objects. No SMP, hotplug, NUMA, reclamation or dynamic page-map teardown.
