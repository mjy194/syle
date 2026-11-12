# Ring3-managed guest prototype checkpoint

The opt-in `managed-guest` target now links the native guest session, manager,
control callbacks, bounded dispatcher, virtual microcode-signature model and
paired session hooks. Normal boot targets are unchanged. The separate Ring3
lifecycle sample performs six bounded slices; success is not Linux boot.

The dispatcher implements the reviewed CPU/CR/MSR/UART/page-fault subset and
stops at its exit budget. The optional IRQ glue has ownership/reservation and
quarantine contracts. APIC advertisement and IRQ injection remain disabled by
default. The newer independent APIC register-model work is not automatically
substituted for the managed target's original guarded device backend.

## Build and host checks

```
sh krnl/tests/guest_control.sh
sh krnl/tests/linux_guest_session.sh
sh krnl/tests/linux_guest_dispatch.sh
sh krnl/tests/linux_microcode_model.sh
sh krnl/tests/linux_irq_glue.sh
make -B -C efi managed-guest MG_INPUTS=/path/to/fixed-linux-inputs
```

Use GNU-EFI (set EFI_PREFIX if nonstandard). Inputs are operator-supplied
bzImage and initramfs.cpio; the make target checks their fixed SHA256 hashes.
They are not stored in this source repository. This target builds kernel and
Ring3 ELF artifacts; it does not launch a guest or produce an acceptance result.

The five host commands produce six passing component/fixture results, including
real LAPIC/IOAPIC model glue. Native privileged behavior received static review;
host fakes and successful linking are not Ring3-to-VMX runtime verification.
The complete frozen combination received independent review and fresh full
recompilation. Linux shell, input/IRQ runtime behavior and USER-02 end-to-end
acceptance still require separate validation. Terminal work is not part of this
checkpoint.

The unified APIC bridge now has compiled FEE/FEC MMIO and MSR routes and uses
paired IRQ hooks. This is still an inactive optional configuration: the
production CPU profile does not advertise APIC and the default startup does
not enable it. Host routing tests are distinct from Linux IRQ acceptance.
