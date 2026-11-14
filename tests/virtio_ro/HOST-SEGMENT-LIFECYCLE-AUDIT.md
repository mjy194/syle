# Bootstrap/VM-exit segment lifecycle audit (not hardware proof)

## Actual source chain

1. `exceptions_init` builds private GDT code/data/TSS descriptors, calls
   `load_gdt_segments`, executes LTR 0x18, builds private IDT and LIDT.
   `load_gdt_segments` loads GDTR, far-returns into CS8, loads data selectors,
   clears FS/GS/LDTR. LTR changes the available TSS descriptor to busy.
2. `exceptions_user_setup` changes RSP0, user GDT entries 5/6 and IDT128 only;
   it does not deliberately replace kernel code/data/TSS or IDT2. This is a
   source lifecycle observation, not a write-protected table guarantee.
3. The managed wrapper enters its host context before session operations.
   The source explicitly notes int80 starts with the Ring3 root and null SS;
   managed_host_context switches the already-owned root/selectors. A gate
   requiring SS16 belongs INSIDE that context, not arbitrary syscall entry.
4. `linux_vcpu_backend.capture` uses SGDT/SIDT/STR and reads current GDT
   descriptors. It validates CS/SS code/data form and busy TSS base/limit.
   This reads descriptor memory; it does not directly read hidden segment
   caches. `linux_vcpu_host` then writes captured host selectors and TR/GDT/IDT
   bases to VMCS host-state fields (including 0x6c0a TR base).
5. `lvn_enter_raw` snapshots GDTR/IDTR before VM entry. On its common return
   path it reloads LGDT/LIDT before POPFQ/RET, restoring table limits as well
   as bases. It does not execute LTR or an explicit CS/SS reload there.
   VM-exit host-segment hidden state therefore depends on architectural VMX
   load semantics and the exact accepted VMCS host fields; the present host
   tests do not execute those hardware transitions.
6. `lvs_run` restores its tracked host MSRs/FPU image and flags host_dirty on
   restoration failure. Session host_safe fail-stops when dirty. Manager safe
   additionally compares CR0/CR3/CR4. It does NOT currently compare hidden
   TR/CS/SS caches or establish an atomic descriptor/mapping generation.

## Other write/load paths that prevent a blanket proof

The repository also contains LTR in smp_cpu and LGDT/LIDT or VMCS host-state
writes in SMP/simple/nested/firmware guest paths. The chosen managed profile
must exclude their concurrent execution and later descriptor replacement, not
merely pass one source grep. AP-start-attempted rejects one class of expansion,
not arbitrary BSP call paths. Old firmware/native evidence cannot be silently
reused for an altered descriptor lifecycle.

## What the refusal candidate actually proves

The candidate compares current visible CS8/SS16/TR0x18 and current private
IDTR/GDTR/vector2/TSS/IST configuration and rejects exception markers. Matching
these fields is necessary but not sufficient: hidden CS/SS/TR state can differ
from subsequently edited descriptor memory. SIDT/SGDT/STR snapshots are not
atomic with NMI, table writes or mappings. The real marker-zero result is not
an exclusion lock. Native mapping remains unconditional panic.

## Minimal further evidence before considering activation

- Establish a private bootstrap-generation latch only after real table loads,
  with all later table writes/load paths either tracked or forbidden by the
  chosen controller. Do not accept a caller-written ready/generation bool.
- Tie actual VMCS host fields and post-exit restoration to that generation;
  verify no path returns to scoped operations after uncertain host state.
- Test actual descriptor-load/VM-exit behavior in a dedicated VM with retained
  snapshots. Host synthetic captures cannot replace this experiment.
- Keep NMI fatal retention/no cleanup on the selected path; audit marker/table
  residency and immutable aliases plus map/borrow synchronization separately.

No lifecycle latch or native admission has been installed by this audit.

## Host chain regression added and independently reviewed

`exception_context_gate.sh` now executes actual exceptions_init/user_setup table
construction in an isolated host copy. Only CLI/LTR/LIDT instructions are
replaced by explicit host hooks; load_gdt_segments is a host hook. These hooks
simulate descriptor accessed/busy effects, not CPU hidden state. The test then
feeds the constructed GDT into actual lvb_descriptor and actual lvh_open, whose
mock capture supplies fixed selectors and whose VMX callbacks record actual
host-field writes. Assertions cover CS8/SS16/TR18 and TSS/GDT/IDT addresses.
The real native capture function and VM entry/exit are not executed.

Independent ASan/UBSan review passed this construction -> parser -> field-writer
chain, the SS16 refusal case, and recompilation from a fresh candidate copy with
original privileged instructions restored. Static lvn_enter_raw disassembly
shows GDTR/IDTR save offsets 160/170 and the final common return's LGDT/LIDT
before POPFQ/RET. These are build/host observations, NOT loaded hidden-cache or
hardware VM-exit restoration evidence.

The isolated final ELF helper relocation also resolves to that ELF's actual
nmi_entered and exception_entered symbols. Runtime IDT ownership and subsequent
asynchronous exclusion still require separate evidence; native map remains
unconditionally fail-stop.
