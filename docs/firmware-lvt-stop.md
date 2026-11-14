# LVT timer write: evidence and next gate

Native evidence after RF completion fix: three MMIO transactions committed
(SVR read, SVR write, LVT timer read). The next request is a32-bit write at
FEE00320, RIP FFFCFC60, qualification582. It is rejected. This is not evidence
that timer countdown or IRQ delivery works.

The pinned CODE bytes at that RIP are89 33: MOV [RBX],ESI. Existing native
logs printed RAX/RCX but not RSI, so they do not establish the write value.
The new read-only fat_trace reports the decoded register/value, transaction
stage, old LVT/initial/armed state. It does not alter admission or device state.
A host test uses the exact opcode with a distinct RAX and RSI to verify that
telemetry reports RSI, including rejection without VM writes or state changes.

Matching source provides a hypothesis, not a substitute for the runtime value:
OvmfPkg/Sec/SecMain.c calls InitializeApicTimer(0,MAX_UINT32,TRUE,5) and then
DisableApicTimerInterrupt(). The selected BaseXApicX2ApicLib first sets periodic,
Mask=0 and Vector=5, writes LVT, then writes initial count. Thus20005 is the
expected first LVT value given the known reset LVT, pending actual telemetry.

## State-machine constraints before expanding timer support

- UNSTARTED: no countdown and no expiry. Register programming and event
  generation are distinct. Allowing a masked/unmasked setting here does not
  justify accepting a nonzero initial count later.
- ARMED_MASKED: only after an explicit reviewed time domain is connected.
  Divider, count, periodic phase and masked expiry must be modeled honestly;
  consuming a masked expiry is different from discarding a deliverable IRQ.
- ARMED_UNMASKED: not supported by current source-free guard. A future stage
  must retain the pending event/illegal-vector outcome or fail-stop before
  consuming it. Never infer safety from a large count, IF0, or an anticipated
  later masking instruction.
- Vector5 lies in reserved exception-vector territory. Programming an LVT
  vector and actually delivering an illegal vector are different events;
  ESR/error-LVT behavior must not be faked. Current guard and timer policy
  reject this configuration, pending a narrowly reviewed register/expiry rule.

No host TSC frequency can be assumed from clock diagnostic leaves15/16, which
were zero inside the outer QEMU. Existing lapic_timer and IRQ models are reuse
candidates, but their synthetic1024 tick basis is not an advertised APIC bus
frequency. No timer semantic change is included in the telemetry patch.

Archbook native tests are paused for the user's Codex restart. The next run
must first collect actual decoded value and stage; keep the current rejection
rather than silently changing masks or dropping an IRQ.
