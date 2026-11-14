# Actual exception-context refusal: next candidate design

The vector2 guard closes an unsafe cleanup entry, but is not an adapter-context
credential. Keep native bounded-map entry fail-stop until this independent
binding is reviewed and tested. No source changes are made by this document.

## Minimal non-mutating marker

The actual assembly already sets `nmi_entered` before changing the spare IST
pointer or calling C, and never clears it because the handler never returns.
A tiny read-only helper in the same assembly unit can return that byte without
exporting writable data. It must not allocate, lock, touch IST, clear NMI
blocking, reset flags or invoke any callback. Nested/emergency paths remain
exactly unchanged. This helper would establish only 'refuse this context', not
permission for allocator/console activity inside NMI.

For the optional scoped adapter, prefer rejecting ANY fatal exception context
using the existing exception_entered marker as well, rather than incorrectly
assuming non-NMI exception cleanup is universally safe. This broad refusal
belongs to the optional adapter; do not silently change ordinary user PF/GP
handling in the narrow vector2 baseline patch. Existing raw manager behavior
is unchanged until a separate optional profile is selected.

## Bind marker to the actually installed entry path

A zero byte alone is insufficient if another IDT/handler is active. A trusted
read-only readiness check must inspect current IDTR and require the installed
kernel IDT, exact vector2 target, selector, present interrupt-gate type and IST3.
Check expected TR/TSS ownership and stable tables before accepting an operation.
Reject an uninitialized/replaced IDT, marker set, AP-start attempted, wrong CPL,
IF/DF set, or not BSP. No caller-supplied 'not in NMI' bool is accepted.
A later NMI after admission must take the non-returning fatal path WITHOUT
cleanup; in-flight ownership/pins remain retained. This still does not prove
console logging is NMI safe; logging failure must not recover into the operation.

Such a check does not exclude DMA, table changes by other agents, or aliases.
Current pin/borrow counters are non-atomic and cannot support multicore/NMI
mutation. The marker and fail-stop policy only make rejection explicit; they
must not be presented as a general NMI-safe allocator or mapping implementation.

## Safe test sequence

1. Isolated candidate helper object: disassemble to confirm a read-only byte
   load/return and no entry/IST changes. Exact source hash ties it to the
   actual exception_2 flag store and fatal-only path.
2. Host readiness matrix using synthetic IDTR/gates/TR/markers, plus actual
   bounded-map host checks; no privileged host instructions.
3. Compile and static-link isolated candidate with original entry assembly,
   keeping the default tree unchanged; retain source/ELF hashes.
4. Only in a dedicated QEMU guest, once the shared firmware test queue is idle:
   QMP inject-nmi into READY/RUNNING management contexts, verify fatal retention
   and no cleanup log/counter. Never target the physical host. This validates
   a guest execution path, not host hardware or general race freedom.

The existing host comparison substituted privileged reads/CLI and a terminating
halt observation; it cannot be called an actual NMI execution test.

## Read-only marker candidate implemented (isolated only)

`exception_marker_candidate.py` now appends a private-data-reading helper to
copies of both actual exception assembly variants. The helper consists of two
byte loads, OR and return; it neither writes flags nor changes IST/entry code.
The Linux variant is tested with LINUX_GUEST_PROBE enabled, including its real
#UD/#GP fixup branch. Only host test copies expose local bytes with objcopy to
check all four value combinations; production candidate nmi_entered stays local.

This is not an atomic cross-core snapshot. If NMI interrupts an operation on
the same CPU, the admitted fatal path must not return or reclaim its owner;
we cannot infer that property from two successful loads. Other cores, marker
writes/reset, alternate IDTs, unknown fixups and page-table changes remain
separate exclusion requirements. The helper's ordinary external-call boundary
and actual assembly loads prevent a C caller from substituting a cached bool,
but they are not a memory-reclamation or hardware-DMA fence. Read-only bytes
must themselves be resident in trusted stable mappings.

The native-map gate remains unconditional fail-stop. No marker-zero result is
currently used to admit a native copy. Next review must bind current IDTR/
vector2/TR/TSS to the helper's actual entry path before considering integration.

## Private table refusal candidate implemented (isolated only)

`exception_context_gate_candidate.py` appends a checker inside a copy of the
real exceptions.c so it can compare its actual private IDT/GDT/TSS/IST objects,
not caller-provided expected addresses. The native compilation reads CS, SS,
RFLAGS, IDTR, GDTR, TR and APIC BSP state; host tests supply synthetic captures.
It rejects marker/panic/AP activity, unexpected visible selectors, descriptors,
gate ownership and IST configuration. The actual marker remains separately
reviewed; the host checker stubs marker values to test both reads and a late
refusal. No combined native execution is claimed.

Hidden TR **and CS/SS** caches are not observable merely from STR/selectors and
current GDT bytes. See HOST-SEGMENT-LIFECYCLE-AUDIT.md for the actual bootstrap,
managed context and VM-exit return chain, and its remaining obligations. This
checker is a refusal/precondition candidate; native map still always panics.
