# Isolated permission probe on actual managed Linux RAM/session, not Linux boot.
# Historical fixed input hash gate remains mandatory. No normal target changes.
LEP_DEFS = -I../krnl -DSYLE_MANAGED_GUEST -DLINUX_GUEST_PROBE
LEP_MODS = $(filter-out linux_guest_startup,$(MG_MODS)) linux_ept_probe
LEP_OBJS = $(addprefix build/lep-,$(addsuffix .o,$(LEP_MODS))) build/lep-startup.o build/lep-native-asm.o build/lep-inputs.o build/lep-main.o build/lep-exceptions-asm.o build/lep-guest-code.o
build/lep-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(LEP_DEFS) -c $< -o $@
build/lep-startup.o: ../krnl/linux_ept_probe_startup.c mg-check-inputs | build
	$(CC) $(KFLAGS) $(LEP_DEFS) -DMG_FIXED_INPUTS_VERIFIED -c $< -o $@
build/lep-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(LEP_DEFS) -c $< -o $@
build/lep-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/lep-exceptions-asm.o: src/linux_exceptions.S | build
	$(CC) -g -fno-pie $(LEP_DEFS) -c $< -o $@
build/lep-guest-code.o: ../krnl/linux_ept_probe_guest.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/lep-inputs.S: mg-check-inputs | build
	printf '%s\n' '.section .rodata,"a",@progbits' '.balign 16' '.global linux_kernel_start,linux_kernel_end,linux_initrd_start,linux_initrd_end' 'linux_kernel_start:' '.incbin "$(abspath $(MG_INPUTS)/bzImage)"' 'linux_kernel_end:' 'linux_initrd_start:' '.incbin "$(abspath $(MG_INPUTS)/initramfs.cpio)"' 'linux_initrd_end:' '.section .note.GNU-stack,"",@progbits' > $@
build/lep-inputs.o: build/lep-inputs.S
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-managed-ept-probe.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(LEP_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/mep-main.o: ../user/mgmt/guest_ept_probe.c | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fpie -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -c $< -o $@
build/mep-entry.o: ../user/mgmt/guest_ept_probe_entry.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/mgmt-guest-ept-probe.elf: build/mep-entry.o build/mep-main.o ../user/mgmt/link.ld
	$(LD) -nostdlib -static -T ../user/mgmt/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-ept-probe
managed-ept-probe: build/krnl-managed-ept-probe.elf build/mgmt-guest-ept-probe.elf
