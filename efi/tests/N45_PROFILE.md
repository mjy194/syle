# N45 sparse 1 GiB physical profile (candidate, not hardware evidence)

Build after sourcing `efi/scripts/cloud-env.sh`:
`make -C efi smp-nested-atomic-gigapage`.
The N43 and N44 targets, runtime includes, payloads and frozen evidence remain
separate. N45 advertises virtual EPT capability bits 16 and 17. No physical
large-leaf capability is required: private EPT02 contains only 4 KiB leaves.

A/B share PDPTE17[1], a WB/IPAT1/AD1 1 GiB source leaf with L1 base zero.
PDPTE[0] retains the low PD/PT subtree for code, guest paging, mailbox and VMX
regions. C retains a 4 KiB alternate primary through PD26/PT27; its sparse
secondary mappings use PT21 and PT22 at PDE1 and PDE511. All eleven registered
source tables remain guest-read-only through every low and high alias.

The guest actually accesses these three translations in each owner:

- GVA f000 -> L2 GPA4000f000 -> L1 GPAf000, ordinary data page15.
  C instead resolves L1 GPA1c000, ordinary data page28.
- GVA30000 -> L2 GPA40200000 -> L1 GPA200000, ordinary data page29.
- GVA31000 -> L2 GPA7ffff000 -> L1 GPA3ffff000, ordinary data page31.

These sample different 2 MiB regions and the final 4 KiB of the 1 GiB leaf.
There is no huge host allocation or claim that unsampled offsets are backed.
Each selected backing is a separately owned ordinary 4 KiB data page, with
page30 separating the latter two allocations in the fixture's inventory.
The authority catalog relocates spans29/31 to the high L1 GPAs without duplicate
HPA spans. Low EPT12 PTE29/31 also resolve those high L1 GPAs, so stopped-owner
TAM can validate every present 4 KiB terminal. Low EPT01 aliases remain ordinary
data and are included in the exact map audit.

Ten new retained 4 KiB PT allocations supply high L1 physical mappings at
PDE1/PTE0 and PDE511/PTE511 in all five retained EPT01/parking maps. The audit
checks every word, requires all other paths absent, checks low and high aliases,
requires unique pinned RW/WB host pages, and includes all ten PTs in the
private-root/VMCS disjointness check. Source-table aliases are never writable.
Allocation failure retains any acquired pages; no release/reclamation is added.

The two-owner budget-two protocol has thirteen checkpoints and two global
reset rounds. Both sparse secondary pages are read each checkpoint and written
only at checkpoints7/12, with owner-distinct values checked after maintenance.
Each secondary private leaf must stay RO/D0 despite shared huge D, then gain
W/D only on its own write. Durable terminal trace checks cover all three GPAs,
including pointer, table HPA, entry GPA, depth and leaf shift. CAS-count auditing
recognizes PDPTE/PDE huge terminals. Source graph inventory requires eleven
pages; global clear/retrack still requires eight reset receipts and epoch3.

## Focused software gates

- `python3 efi/tests/nested45_memory.py --sanitizer none`
- Repeat with `--sanitizer address,undefined` and `--sanitizer thread`.
- `python3 efi/tests/nested45_integration.py`
- `python3 efi/tests/nested45_freestanding.py`

The memory gate creates fresh source snapshots and runs three executables:
constructor/all-alias ownership, production owner faults, and actual fixture
production two-round TAM/NOAM maintenance. The last exercises all four retained
roots and independent far-offset promotions after each reset. Physical
allocation, VMCS, INVEPT and host mapping probes are mocks in host tests.
The integration gate inspects preprocessed actual profile wiring. The fresh
freestanding gate compiles and links production objects, checks unresolved
symbols, and verifies identical ELF/ESP copies. Source hashes and logs are
retained in the printed evidence directories.

None of these checks executes a physical nested guest. No hardware pass, full
regression, DMA support, dynamic source update, reclamation or other-VM support
is claimed. Hardware acceptance remains a separate exact-image gate.
