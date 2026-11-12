# BSP exception and output boundary

The diagnostic guarantee starts after `exceptions_init` loads the kernel-owned
IDT in `kernel_runtime`, before boot-info interpretation and framebuffer binding.
The brief firmware-table-to-kernel-table transition is not claimed recoverable.
Only BSP is supported here. IF remains zero; no timer or device driver is implied
by populating all 256 present, DPL0 interrupt gates.

Vectors 0–31 use architectural error-code normalization. 32–255 are terminal
fallback entries, except the existing later Ring3 syscall installation at 128.
Normal faults, #DF and NMI use separate ISTs. NMI switches its next IST target to
a spare stack before software pushes. Stack-free entry guards terminate further
entry before C. The hardware itself always writes an interrupt frame before an
entry can inspect a guard; the NMI spare protects the first active NMI frame.
There is no IRET on these terminal paths, so a handled NMI stays blocked.

Panic emits a full bounded UART-only pass before trying framebuffer MMIO. A
failed display attempt can terminate in the early reentry guard, preserving the
already serialized evidence. This is best effort, not recovery from arbitrary
machine failure. An exhausted UART polling budget disables it; no timer is
needed. Missing both channels still reaches CLI/HLT. Panic with a corrupt
boot-info header uses an independent bounded legacy-UART probe. The boot-info
pointer itself must refer to loader-owned readable memory.

## Automated evidence

- `efi/tests/ms01_vector_layout.py`: all 256 stack layouts, error-code set and
  assembly guard placement, **static**, not synthetic INT delivery of #CP/#VC/#SX.
- `efi/tests/console_memory.sh`: ASan/UBSan public framebuffer binding, format,
  geometry, overflow, bounds, wrapping, scroll and minimum valid surface.
- `efi/tests/ms01_uart.sh`: exact production polling/output code with host-mocked
  register reads/writes, including stalled UART disable. Not a physical UART test.
- `efi/tests/ms01_diagnostics.py`: immutable ESP+ELF QEMU/OVMF/TCG snapshots;
  CPU-executed #UD/#GP/#PF, actual #DF caused by failed ordinary IST delivery,
  software INT255 fallback, output-channel loss, display page fault, and NMI.
  Reads back every gate address, CPU protection state, vector/error/RIP/selected
  saved registers, and frame bounds within the corresponding IST. #DF saved RIP
  is architecturally undefined and deliberately not asserted. No-UART asserts
  actual framebuffer panic pixels. All terminal tests assert IF=0 and HLT.
- `nmi-repeat-request` submits a second hardware NMI request while an NMI handler
  is active; architectural blocking applies. `nmi-repeat-guard` is explicitly a
  debugger-forced early-guard state plus a hardware NMI, **not** proof of natural
  hardware NMI reentry. `nested-guard` is similarly synthetic. `screen-fault`
  exercises a real second #PF during panic rendering and preserves UART evidence.
- `efi/tests/ms01_cpu_boundary.py`: corrupt ABI, invalid framebuffer normalization,
  WP/NXE normalization and actual TCG CPUID without NX. These are emulation and
  adverse-state injection, not physical-platform certification.

Run sequentially if using default debugger port 1234. All tests retain serial,
GDB command/output and JSON reports under `efi/build/ms01-*`.
