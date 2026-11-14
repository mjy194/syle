# Bounded physical-map candidate (native entry blocked)

The optional native_map_candidate backend keeps translated pointers inside a
synchronous copy. Its host tests use the real paging_page_rw_wb walker,
paging_phys_to_virt conversion, real MTRR snapshot policy, actual PMM authority
and scoped pin/borrow state. Low mmap-backed pages and synthetic CR3/CR4/PAT
stand in for hardware. Read-only mappings and a mismatched CR3 reject writes;
valid last-byte read/write succeeds. External buffers are explicitly admitted
and cannot alias the target or authority metadata. A successful page query is
not proof that other host/EPT aliases do not exist.

The native gate is deliberately unconditional fail-stop with a diagnostic.
There is NO caller boolean or build-time success flag that makes it run natively.
The host-only gate is solely a test seam. Current NMI context cannot be reliably
queried by this adapter, and a first CPL3 NMI can reach owner cleanup in baseline
code. Even the vector2 guard candidate only removes that call; it does not
establish a native NMI-entry marker, mapping stability or multicore exclusion.

Table/PAT/MTRR immutability, TOCTOU exclusion, admitted host-buffer residency,
external alias exclusion and device isolation remain trusted/native blockers.
No dynamic map/unmap is performed: paging_phys_to_virt is an existing mapping
conversion, not a new lease. The adapter returns no owned pointer, but prior
raw-map users must be retired before claiming scoped lifetime.

Run native_map_candidate.sh for host checks. NATIVE_MAP_LINK=1 includes the
blocked native adapter and scoped/authority support in an isolated final ELF;
it does not select it in the default manager or enable guest DMA. Combining
NMI_GUARD_LINK=1 does not disable the adapter's fail-stop gate.
