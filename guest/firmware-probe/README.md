# Reset-mode native probe (not OVMF)

Build the supervisor-only diagnostic kernel:

```
cd efi
make -f Makefile -f firmware-probe.mk firmware-probe
```

Use `build/krnl-firmware-probe.elf` in a NEW isolated ESP with the usual Syle EFI
loader and required management ELF file (the loader still validates its inputs;
this probe never invokes the manager). Keep the known QEMU KVM/nested VMX CPU,
1 vCPU, 512 MiB, private OVMF VARS, no network, and external timeout. Include the
existing `isa-debug-exit` at port F4. The outer QEMU OVMF remains Syle's loader;
inside Syle the guest is this trusted 64KiB ROM, not OVMF.

Success requires three ordered output markers 16,32,64, followed by final
`FIRMWARE.PROBE PASS reset16->protected32->long64 cleanup`. HLT is accepted only
in long mode with CS.L set. Main emits debug-exit 0x10 (QEMU status 33) on success,
0x11 (status 35) on failure. Missing VMX/UG or failure at any stage is NOT success.
Every native entry is preemption bounded and the entire fixture is limited to
4096 exits. Any partial VMCS mode commit stops execution. Cleanup must complete
VMXOFF before releasing owned firmware/EPT pages; uncertain host state or
ownership fatally stops instead of pretending to reclaim it.

The fixture builds its own identity page tables for 0..4GiB in owned low RAM.
Only low128MiB and the high ROM/VARS suffix have EPT backing. It uses no firmware
services and makes no claim about OVMF, PCI, guest disks, display or M2 acceptance.
Its CR model is deliberately restricted to entering long mode, not leaving it.
The production Linux-only opener/dispatcher remain unchanged.
