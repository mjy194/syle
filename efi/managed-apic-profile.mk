# Additive opt-in: make -f Makefile -f managed-apic-profile.mk managed-apic-profile
# Requires the current managed-guest.mk already included by Makefile.
# Never a dependency of all/default; never executes a VM. Private object prefix
# prevents mixing previously compiled baseline startup with experimental flags.
MP_DEFS = $(MG_DEFS) -DMG_EXPERIMENTAL_APIC
MP_MODS = $(sort $(MG_MODS) linux_pit2 linux_nonpresent_pf linux_profile_service linux_profile_prepare linux_profile_activation linux_profile_gate linux_cpu_profile linux_acpi_reduced_install linux_acpi_reduced linux_acpi_topology linux_acpi_install linux_apic_dispatch linux_apic_mmio linux_ioapic_mmio linux_apic_msr linux_mmio_memory guest_mmio_mov guest_apic lapic_timer linux_guest_irq_adapter linux_guest_irq_glue)
MP_OBJS = $(addprefix build/mp-,$(addsuffix .o,$(MP_MODS))) build/mp-native-asm.o build/mg-inputs.o build/mp-main.o build/mp-exceptions-asm.o
build/mp-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(MP_DEFS) -c $< -o $@
build/mp-linux_guest_startup.o: ../krnl/linux_guest_startup.c mg-check-inputs | build
	$(CC) $(KFLAGS) $(MP_DEFS) -DMG_FIXED_INPUTS_VERIFIED -c $< -o $@
build/mp-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(MP_DEFS) -c $< -o $@
build/mp-exceptions-asm.o: src/linux_exceptions.S | build
	$(CC) -g -fno-pie $(MP_DEFS) -c $< -o $@
build/mp-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-managed-apic-profile.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(MP_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-apic-profile
managed-apic-profile: build/krnl-managed-apic-profile.elf
