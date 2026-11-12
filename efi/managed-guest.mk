# Explicit Ring3-owned guest session. Never part of default all; never runs VM.
MG_INPUTS ?= ../../inputs
MG_DEFS = -I../krnl -DSYLE_MANAGED_GUEST -DLINUX_GUEST_PROBE
MG_MODS = pit0 pic0 pic0_ports pic0_glue pit0_transaction guest_wait legacy_irq_runtime linux_rtc linux_pause linux_pic_mask linux_debug_disabled linux_sysenter_msr linux_invlpg linux_disabled_apic linux_pit2 linux_pf_diagnostic linux_profile_activation linux_profile_gate linux_cpu_profile linux_acpi_reduced_install linux_acpi_reduced linux_acpi_topology linux_acpi_install linux_nonpresent_pf linux_guest_irq_glue linux_apic_dispatch linux_apic_msr linux_apic_mmio linux_ioapic_mmio linux_mmio_memory linux_guest_irq_adapter guest_apic lapic_timer guest_mmio_mov linux_irq_glue linux_external_irq linux_irq_adapter lapic ioapic linux_boot linux_guest_memory linux_vcpu_prepare linux_vcpu_host linux_vcpu_backend linux_vcpu_state linux_vcpu_native linux_recovery_mem linux_guest_session linux_guest_manager linux_guest_startup linux_guest_dispatch linux_guest_snapshot linux_cpu_commit linux_vcpu_policy linux_vcpu_cr linux_gs_commit linux_vcpu_msr linux_efer_commit linux_msr_bank linux_microcode_model linux_pf_guard_v5 linux_vcpu_event linux_high_pf linux_high_pf_data linux_pt_diag linux_pt_permissions linux_uart_runtime linux_uart_model_io linux_uart
MG_OBJS = $(addprefix build/mg-,$(addsuffix .o,$(MG_MODS))) build/mg-native-asm.o build/mg-inputs.o build/mg-main.o build/mg-exceptions-asm.o
.PHONY: mg-check-inputs managed-guest
mg-check-inputs:
	printf '%s\n' '3d1c081c18e64ad97a44e2b038b8b3d0c733d38494b6293ba2402898c3a629e6  $(MG_INPUTS)/bzImage' '7f71f66ed40ca0fa5e26e9dba8138672c148b0f720ed856a118b8955c36fa0a5  $(MG_INPUTS)/initramfs.cpio' | sha256sum -c -
build/mg-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(MG_DEFS) -c $< -o $@
build/mg-linux_guest_startup.o: ../krnl/linux_guest_startup.c mg-check-inputs | build
	$(CC) $(KFLAGS) $(MG_DEFS) -DMG_FIXED_INPUTS_VERIFIED -c $< -o $@
build/mg-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(MG_DEFS) -c $< -o $@
build/mg-exceptions-asm.o: src/linux_exceptions.S | build
	$(CC) -g -fno-pie $(MG_DEFS) -c $< -o $@
build/mg-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/mg-inputs.S: $(MG_INPUTS)/bzImage $(MG_INPUTS)/initramfs.cpio | build
	printf '%s\n' '.section .rodata,"a",@progbits' '.balign 16' '.global linux_kernel_start,linux_kernel_end,linux_initrd_start,linux_initrd_end' 'linux_kernel_start:' '.incbin "$(abspath $(MG_INPUTS)/bzImage)"' 'linux_kernel_end:' 'linux_initrd_start:' '.incbin "$(abspath $(MG_INPUTS)/initramfs.cpio)"' 'linux_initrd_end:' '.section .note.GNU-stack,"",@progbits' > $@
build/mg-inputs.o: build/mg-inputs.S mg-check-inputs
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-managed-guest.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(MG_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/mg-lifecycle.o: ../user/mgmt/guest_lifecycle.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/mgmt-guest-lifecycle.elf: build/mg-lifecycle.o ../user/mgmt/link.ld
	$(LD) -nostdlib -static -T ../user/mgmt/link.ld $< -o $@
managed-guest: build/krnl-managed-guest.elf build/mgmt-guest-lifecycle.elf
