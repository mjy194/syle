# Firmware serial console and UEFI Shell commands

The opt-in console profile exposes a synthetic PCI 16550 function at 00:02.0.
OVMF discovers it through its ordinary PCI serial driver and adds the terminal
to ConIn/ConOut. The guest executes its own Shell commands. No command is
written into guest RAM, and no output is synthesized by the console model.

## Implementation

`firmware_console.c` supplies a type0 configuration header, an eight-byte I/O
BAR with sizing/assignment and an I/O-enable command bit. Memory decode and
bus mastering remain hardwired off. Configuration accesses are committed with
RIP; uncertain VMWRITE stops the observer. The device uses the existing
`linux_uart` register/FIFO model for byte IN/OUT, DLAB, FIFO reset and modem
loopback. IER must remain zero: this firmware profile uses polling and the
existing periodic APIC interrupt wakes HLT. It has no new UART IRQ route.

The adapter validates actual owned instruction bytes and DX, under the existing
CPL0/mode/event gates. UART/RAX and transmitted bytes publish only after RIP
commit. Host COM1 is polled while the guest is stopped, with capacity checks,
line-error rejection and no waiting for input. Guest accesses never directly
execute against host UART registers. Transmitted bytes have a bounded 16KiB
capture and live output through the admitted outer debug console.

Native builds retain the prior VMX slice, 131072-dispatch, 30-second reference
and 2MiB trace bounds. Once UART decode is enabled, repetitive observation is
sampled every 256 dispatches. All guest TX bytes are retained independently;
final records and dispatch rejection are outside that sampling. The probe
stops after both commands arrive and the expected marker/prompts appear;
the external analyzer verifies standalone results and the full trace footer.

Only after guest execution stops, the console target increases the diagnostic
UART polling bound from 10000 to 1000000 iterations per character. This lets
the socket consumer be rescheduled during the final dump. Other targets keep
the existing bound. The continuous socket reader records host input separately.

## Validation

The host UART/configuration suite passes ASan/UBSan: PCI discovery, BAR sizing,
I/O gating, all 256 byte values in both directions, DLAB, RX-full backpressure,
nonzero IER rejection, wrong opcode and partial-VMWRITE terminal behavior.
The existing UART suite passes. Checkout and frozen-runtime builds pass, with
no undefined kernel symbols. The production ELF loader accepts the final
approximately 5.2MB file and 7.7MB span, below its 5MiB/8MiB limits.

The native environment was re-admitted after the host upgrade to
7.3.4-arch1-2. Current module hashes/source versions, unchanged QMP PCI/device
admission, fresh media/VARS, no networking and exclusive attempt markers are
recorded in each run. Each positive run is preceded by tiny exit33 and no-Hv
exit35 controls. Diagnostic exit35 remains the firmware observer convention.

The first console candidate printed the Shell banner but exhausted trace
capacity before the prompt. Routine sampling let subsequent runs execute
`ver` and `echo SYLE_CONSOLE_OK_20261117`, returning to `Shell>` after each.
Two captures exposed the short diagnostic UART polling bound and lost the
end of the dump; those incomplete records remain preserved. The final target
adds the post-execution drain bound described above. That produced a complete
trace; its initial stop recognizer missed the ANSI-interleaved command echo,
so the run continued to its reference-time bound. The accepted probe uses the
standalone result line and third prompt instead.

Evidence directories are `efi/build/firmware-console-20261117`,
`efi/build/firmware-console-20261117-sampled` and
`efi/build/firmware-console-20261117-final` and
`efi/build/firmware-console-20261117-accepted`. The accepted directory contains source
and media manifests, native runners, command input, raw serial, analysis,
build/test/ELF logs and the evidence archive. Reproduction preparation is in
`experiments/m2-snapshots/console-progress/README.md`.

This acceptance covers a polled serial UEFI Shell. Firmware-launched Linux,
storage, display and Ring3 lifecycle acceptance still belong to milestone2.

The binding assumptions were checked against the official EDK2
[PCI serial driver](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Bus/Pci/PciSioSerialDxe/Serial.c)
and [console device registration](https://github.com/tianocore/edk2/blob/master/OvmfPkg/Library/PlatformBootManagerLib/BdsPlatform.c).
Native output from the pinned CODE image is the acceptance evidence.

## Accepted native result, 2026-11-17

The final isolated run passed both controls and QMP admission, delivered all
34 host input bytes, captured 2,038 guest TX bytes, executed `ver` and
`echo SYLE_CONSOLE_OK_20261117`, and returned to three Shell prompts. The
standalone echo result is verified separately from its command echo.
The probe stopped intentionally, with a complete zero-loss trace and cleanup.
Five damaged evidence variants are rejected by the analyzer.

Across this follow-up, five positive native attempts move the recorded
architectural WBINVD ledger45→50; this is not a physical-flush count.

Final dispatches: 91,189/131,072; encoded trace: 1,178,850/2,097,152 bytes. Outer process elapsed: 55.264s, no timeout.
