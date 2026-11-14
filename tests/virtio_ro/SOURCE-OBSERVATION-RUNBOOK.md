# Passive source permissions observation: NOT YET RUN

Use only mode 0 (READY wait) of the previously frozen isolated NMI kernel/manager
bundle, but this new runner has NO injection operation. This is a passive
observation of that exact baseline kernel, not a boot of the new managed bounded
static ELF. Reuse the fresh-media assembly/hash procedure from the reviewed NMI
runbook, not an old VM/process/socket or modified variables disk.

Freeze run_source_observation.py, source_page_audit.py, source-inventory.json,
inventory-local.json, preflight-baseline.json, original kernel/manager/media
hashes together. Generate source inventory from the exact ELF; command literal
must occur once in its rodata and actual session.command_line must equal that
address. Actual session image/initrd pointer+length are also checked. Inventory
layout compilation must match that frozen session layout. Source bytes are hashed
by exact symbol/literal length across resolved physical segments.

The captured `address_limit` comes from the frozen actual PMM CPU-limit code
(CPUID.80000008) and may be clamped DOWN by PMM_DIRECT_MAP_LIMIT. It is a
conservative allowed address bound, NOT an exact MAXPHYADDR assertion. A page
entry using higher bits is rejected even if another CPU could legally support
it. Freeze/review the code path and unique data symbol before running.

Launch is fresh QEMU -S, one vCPU, no NIC/passthrough, private QMP. The runner
retains exact executable/argv/PID starttime checks, socket inode and SO_PEERCRED,
QMP CPU thread association and fixed device-map comparisons. It must be scheduled
by inputs/root AFTER the firmware KVM queue is idle. It is not self-authorized to
start concurrently. No execution is part of generating or testing this runner.

After query-status running=false (prelaunch/paused), cont only to the existing
READY_WAIT marker, STOP event acknowledged and running=false, exact CPL3/RIP
wait boundary, controller/session state checked. It never resumes afterwards.
All memory reads are guest physical pmemsave into new private files. Before ANY
snapshot/read, capture stopped `info mtree -f` and parse the unique system-memory
FlatView. Only regions typed RAM and named pc.ram, intersected with
[1 MiB, 512 MiB), are whitelisted. VGA/ROM holes, other RAM names, overlap or
unsupported text fail closed; an upper address bound alone is NOT a RAM map.
Every pmemsave and exact hex-only HMP xp checks this whitelist. All pmemsave
filenames must be new direct .bin children of the private mode directory;
existing/dangling symlinks or files are rejected, including before-snapshots. At most 4096 distinct table pages and 8192
cached physical pages, absolute overall 120-second deadline. Budget/unsupported
mode/truncation/error is INCONCLUSIVE, not 'no aliases'; always quit/terminate
and reap own process. No auto retry/host settings changes/NMI.

The walk is Syle's current CPL3 CR3: QEMU guest virtual -> QEMU guest physical.
It is NOT nested Linux EPT, guest Linux CR3 or host physical translation. Require
PG/PE/WP, PAE, LMA/NXE; reject LA57/PCIDE/PKE/PKS and unknown address/reserved bits.
RW/US accumulate by AND and NX by OR, including huge leaves. Repeated table PA
fails inconclusive rather than skipping a distinct VA/permission alias path.
Alias reports concern only physical overlap with exact source byte spans among
this snapshot's fully traversed reachable mappings. Other CR3s, future table
changes, VMX alternate roots and devices/DMA remain outside scope.

Even a full snapshot without writable aliases does NOT authorize native copy,
provide source residency lifetime or prove general NMI safety. Keep native gate
refusal/panic unchanged. Human review must check registers, page-table evidence,
source hashes and any aliases in addition to the JSON result.

Parser currently rejects 1 GiB leaves in the runner because no independent
PDPE1GB feature capture is available; synthetic parser tests may explicitly
supply support. This is a conservative supported-profile limit, not a malformed
claim about all 1 GiB mappings. Other legal ignored/software bits may likewise
be rejected conservatively. Such refusal yields no permission conclusion.

Host runner tests extract only the actual identity/query functions without
executing top-level code: PID/starttime/executable/argv/socket inode mismatch
must reject; injection/device mutation/HMP writes must reject. These tests do
not exercise live SO_PEERCRED, QEMU launch, STOP event timing or process cleanup.
Those code paths retain the reviewed base behavior but need this new runner's
independent review before any preflight run.

Before importing helper modules or launching QEMU, the runner itself validates
all CONTROL.sha256 entries, rejects duplicate/invalid names and symlinks, and
requires the runner/parser/policy/inventories/baseline/frozen artifacts to be
covered. The externally reviewed CONTROL digest still establishes provenance;
rewriting both controls and hashes is not a trust mechanism. Retain the stopped
FlatView text and admitted ranges with the result. Unknown QEMU FlatView syntax
requires investigation/review, never fallback to an assumed RAM window.

## First real observation stopped safely; exact-format v2 correction

The first run stopped INCONCLUSIVE before all snapshots/pmemsave because actual
QEMU 11.1.3 emits the separate ` KVM` marker following pc.ram's backing offset.
Raw archive SHA f137ba257cd1457c077fa8a1c6c7e5d13ee03a949952aaca18d5bffb5f934ae6;
raw FlatView fixture SHA 550660bcd6f4b376441d30ff661849b7fcc372c712c03e79f0b6960bc718794c.
The transcript independently contains zero pmemsave and zero inject-nmi calls.
There is no source permission/hash result from that run.

The corrected parser recognizes ONLY an optional exact trailing ` KVM` marker,
with pc.ram still required to be `ram` in the unique system-memory FlatView,
nonoverlapping against every listed effective region. An explicit 16-hex-digit
backing offset must equal the region's system start (missing offset means zero);
other pc.ram aliases are conservatively unsupported. Actual system FlatView #3
has pc.ram [0x100000,0x20000000), while VGA/ROM and high MMIO regions are excluded.
Other FlatViews (including KVM-SMRAM and device bus masters) are not substituted
for the system view. KVM marker alone never grants read permission.

The exact raw fixture is retained with host regression tests, including malformed
marker, wrong offset, RAM-name-but-I/O-type and overlapping region negatives.
This narrowly fixes real text parsing, not source-memory safety. Any future
unknown text/region still refuses; only root may schedule a fresh reviewed run.
