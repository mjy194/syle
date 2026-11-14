# Restricted PIIX4 PM observation integration

Native diagnostic chain is now established: enabled config cycles for BDF00:01.3 return FF at80h and FFFFFFFF at40h; later IN dword at port0006 executes opcode ED at RAM RIP8246FC. Pinned OVMF computes `(PMBA & ~1)+8`; absent-PMBA data explains the invalid port. This integration does not implement port0006.

The independent clock observer routes exact BDF00:01.3 PMBA40h/dword and PMREGMISC80h/byte to the reviewed PM model. Reset PMBA1/decode0 permits the real firmware constructor to configure B001 and enable bit0, yielding B008. Other configuration fields, including a complete PCI identity/header, are intentionally not synthesized; accesses stop rather than claim the function is fully compatible or absent. Other BDFs retain existing discovery behavior. No bus mastering, SMBus, SCI, SMI, sleep, PM1 enable or status-clear interface is offered.

The 24-bit PM timer uses 3,579,545Hz derived from the same admitted Hv10MHz origin/epoch as RTC and the chosen100MHz APIC. Entry/exit boundaries and I/O sample elapsed reference time. The internal TMR_STS latch is sticky across bit23 toggles, including multiple wraps, and is never discarded by a counter read. A request to enable unsupported SCI or to read/clear unimplemented PM1 registers stops. No FADT has been generated or validated: its eventual timer address/length/width flag must match this device before end-to-end claims.

PCI and PM I/O transactions require exclusive ownership, CPL0, supported scalar width/encoding and canonical RIP advancement. Source sample effects persist before staging. Configuration/AL or EAX publish only after successful RIP commit; a failed or uncertain VMCS write permanently prevents reentry. The observer returns directly after this adapter, avoiding a second RIP advance. Diagnostics retain config cycles and new PM base/enable/tick/status fields.

Test requirements: host `firmware_pmtimer.sh` and `firmware_piix_pm.sh`; normal PCI fixture regression; build/nm/ELF checks; then serialized tiny, no-Hv source-negative and Hv-positive native observation. This is an incremental diagnostic subset, not a complete PIIX4 chipset or completed firmware boot.

## Word-I/O regression correction

The first native PM integration stopped earlier than intended on the existing host-bridge DID read: CF8=80000000, portCFE, IN word, bytes66 ED. The adapter incorrectly required length1 for every DX form, so it rejected before old `fpi` lane handling. This was an integration regression; it did not validate PMBA or PM timer operation.

The corrected entry fetches actual instructions only through the reviewed owned-RAM/CODE page walker, with native CS.L=1 and CS.D=0. The scalar decoder accepts exact unprefixed byte/dword or single66-prefixed word IN/OUT forms, and matches direction, width, VMX length, immediate-vs-DX and immediate port. Address-size override, REP, REX, duplicate or other prefixes are deliberately unsupported and stop. Word DX advances RIP by2; word immediate by3; byte/dword use1/2. No assumption is made that all valid x86 encodings are supported.

Real-entry regression tests exercise portCFE returning device1237 while preserving upper RAX bits, alongside original dword vendor/device read and prefix/direction/truncation negative cases. The original PCI discovery model is unchanged. Native rerun must establish that the old host-bridge path is restored before claiming any PM progress.
