# Preserve RF on pure preemption resume

Native reproduction with read-only diagnostics: VMX preemption exit reason52, RIP FFFD45B9, RFLAGS10006 (RF=1), interruptibility0, pending-debug0, CS38. Earlier runs sometimes passed this location, so the original stop was timing-dependent, not fixed by logging.

Intel SDM vol3C section28.3.3 describes saved RFLAGS.RF. EPT/APIC-access and some instruction-caused exits have specific RF rules; for other exits the processor saves the RF value that existed immediately before the exit. VMX preemption does not imply an emulated/completed guest instruction. Unlike the earlier emulated MOV completion path, this resume path must not clear RF or advance RIP. The next real guest instruction handles RF according to normal architectural execution.

The independent gate allows only reason52, CPL0, fixed RFLAGS bit1, no reserved/high bits or virtual8086 mode, TF=0, DR7 exactly400h (no local/global enabled breakpoints or general-detect), no pending debug, no interruptibility blocking, no IDT-vectoring state and no pending entry event. It is deliberately narrower than all architecturally resumable states. Unsupported debug/event states stop rather than being erased.

On success the observer returns to its loop with all guest VMCS state unchanged. It resamples the timer at the next entry boundary and reloads only the host scheduling/preemption budget. It performs no RIP/RFLAGS write and does not claim instruction completion. The generic emulation gate and EPT RF-completion code remain unchanged. Tests cover measured RF1, RF0, other exit reasons, CPL, single-step, DR7 breakpoint/GD bits, interruptibility, pending debug/vectoring/entry event, reserved flags and missing fixed bit1. Native coverage still requires a reproduced RF1 exit to pass this gate.

Reference: https://cdrdv2-public.intel.com/671506/326019-sdm-vol-3c.pdf (section28.3.3). Stored text lines around6397–6407 provide the RF exception cases and the general preservation rule.
