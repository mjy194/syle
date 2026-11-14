# Additive opt-in: make -f Makefile -f managed-native-pf.mk managed-native-pf
# Requires the current managed-guest.mk already included by Makefile.
# Never a dependency of all/default; never executes a VM. Private object prefix
# prevents mixing previously compiled baseline startup with legacy PIC flags.
MNP_DEFS = $(MG_DEFS) -DMG_LEGACY_PIC04 -DMG_PROGRESS_DIAG -DMG_NATIVE_GUEST_PF
MNP_MODS = $(sort $(MG_MODS) linux_progress_diag pit0 pic0 pic0_ports pic0_glue pit0_transaction guest_wait legacy_irq_runtime)
MNP_OBJS = $(addprefix build/mnp-,$(addsuffix .o,$(MNP_MODS))) build/mnp-native-asm.o build/mg-inputs.o build/mnp-main.o build/mnp-exceptions-asm.o
build/mnp-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(MNP_DEFS) -c $< -o $@
build/mnp-linux_guest_startup.o: ../krnl/linux_guest_startup.c mg-check-inputs | build
	$(CC) $(KFLAGS) $(MNP_DEFS) -DMG_FIXED_INPUTS_VERIFIED -c $< -o $@
build/mnp-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(MNP_DEFS) -c $< -o $@
build/mnp-exceptions-asm.o: src/linux_exceptions.S | build
	$(CC) -g -fno-pie $(MNP_DEFS) -c $< -o $@
build/mnp-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-managed-native-pf.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(MNP_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-native-pf
managed-native-pf: build/krnl-managed-native-pf.elf
# Build the unchanged bounded output frontend; do not alter the ordinary target.
include managed-shell-probe.mk
managed-native-pf: build/mgmt-guest-continuous.elf

.PHONY: managed-native-pf-probe
managed-native-pf-probe: build/krnl-managed-native-pf.elf build/mgmt-guest-shell-probe.elf
