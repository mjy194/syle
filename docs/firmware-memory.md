# Private firmware RAM and EPT backing

`firmware_memory.c` is independent of the direct Linux memory loader. It prepares
128 MiB private low RAM plus a suffix of the 32-bit physical address space
containing at most 4 MiB of flash. `firmware_layout` defines the candidate map.
This is not a complete PC memory map; future legacy/MMIO holes need an explicit
platform plan rather than being inferred from contiguous low RAM.

The CODE pages are EPT WB/RX; VARS pages are WB/R, so writes exit for a future
flash model rather than silently mutating them. Ordinary RAM is WB/RWX. Guest
page tables retain responsibility for guest-internal virtual permissions. Every
remaining EPT entry stays zero. The high flash maps through its own PD/PT path,
not through an alias pretending to belong to the old Linux backing array.

`fwm_prepare` copies trusted immutable image bytes into newly allocated pages.
`fwm_owned_page` covers RAM, flash and EPT tables, for VMX-control-page alias
rejection. The hash table is an ownership index, not an EPT mapping or proof of
allocator trust. Returned duplicate/invalid physical page tokens quarantine the
whole context before map/writeback/zero touches them, retaining existing tokens
rather than guessing which pages may safely be released. A quarantined context
must not be reset/reused by ordinary code.

On ordinary allocation/map/writeback failure, all unique allocations are
unwound. A failed free remains recorded and a later release can retry it. When
release partially succeeds the ownership index is rebuilt from retained tokens,
so deleted open-addressed entries cannot hide live tokens. If installed in any
VMCS, the caller must first establish VMCLEAR, INVEPT, VMXOFF and absence of all
vCPU/device references. The module does not perform or claim those operations.

ASan/UBSan tests cover the 64 KiB reset ROM and full 4 MiB OVMF split, low RAM,
high ROM permissions/content, absent GPA holes, boundary copies, 24 allocation /
map / WB failure positions, final table-construction map failures, release
failure/retry, and duplicate/invalid allocator tokens. The freestanding compile
also passes. These are host tests, not firmware VM-entry or OVMF acceptance.

Run: `sh krnl/tests/firmware_memory.sh`.
