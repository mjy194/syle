# Firmware observation: reuse the cold/masked PIC model

The fixed OVMF next writes byte FF to port21. The observer now reuses the
existing linux_pic_mask module unchanged, rather than duplicating a PIC or
connecting Linux IRQ0/PIT machinery. Master/slave IMR reset to FF/FF for the
new guest. Byte IMR reads return stored state; valid writes change it.

Supported ICW sequence is narrowly defined by the existing model: ICW1=11,
aligned vector>=20, cascade master04/slave02, ICW4=01 or03. Only fully masked
slave and masterFF orFB are supported operational masks. Opening cascade with
the slave fully masked does not enable any input. Initialization/transient
zero masks require guest IF0 at each I/O and before re-entry. There is no
interception of every STI within a slice; safety relies on NO connected PIC
inputs, NO pending IRR/ISR and NO injection source, not on this entry check
being instruction-by-instruction enforcement.

OCW, EOI, IRR/ISR command reads, unsupported masks/modes and other IRQ behavior
remain fail-stop. AEOI is recorded by the initialization model but no delivery
or acknowledgment behavior is claimed. No host PIC I/O is performed. APIC is
still absent from the CPU profile. No timer, host IRQ or UART IRQ is connected.

The model stages state and register values, commits RIP through a checked
callback, then commits state/AL. On failed or uncertain VMWRITE, it sets the
failed latch and never resumes, not a claim that hardware RIP was rolled back.
The observer returns directly from this branch because the model already
advanced RIP; other I/O follows the prior PCI path. Actual guest RFLAGS and
CPL are supplied; wrong width/qualification/DX/length are rejected.

Tests include the observed OUT21ff, slave masking, readback, initialization,
IF restrictions, invalid unmask/EOI, failed RIP write, reset and unsupported
DWORD operations. Existing Linux PIC tests remain independent regression tests.
All masked-path behavior is a restricted normal-boot contract, not complete
8259 interrupt delivery. A future source requires a real IRQ model and review.
