# Additive opt-in: make -f Makefile -f managed-diagnostic-pf.mk managed-diagnostic-pf
# Requires the current managed-guest.mk already included by Makefile.
# Never a dependency of all/default; never executes a VM. Private object prefix
# prevents mixing previously compiled baseline startup with legacy PIC flags.
MDP_DEFS = $(MG_DEFS) -DMG_LEGACY_PIC04 -DMG_PROGRESS_DIAG -DMG_DIAGNOSTIC_GUEST_PF
MDP_MODS = $(sort $(MG_MODS) linux_progress_diag pit0 pic0 pic0_ports pic0_glue pit0_transaction guest_wait legacy_irq_runtime)
MDP_OBJS = $(addprefix build/mdp-,$(addsuffix .o,$(MDP_MODS))) build/mdp-native-asm.o build/mg-inputs.o build/mdp-main.o build/mdp-exceptions-asm.o
build/mdp-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(MDP_DEFS) -c $< -o $@
build/mdp-linux_guest_startup.o: ../krnl/linux_guest_startup.c mg-check-inputs | build
	$(CC) $(KFLAGS) $(MDP_DEFS) -DMG_FIXED_INPUTS_VERIFIED -c $< -o $@
build/mdp-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(MDP_DEFS) -c $< -o $@
build/mdp-exceptions-asm.o: src/linux_exceptions.S | build
	$(CC) -g -fno-pie $(MDP_DEFS) -c $< -o $@
build/mdp-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-managed-diagnostic-pf.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(MDP_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-diagnostic-pf
managed-diagnostic-pf: build/krnl-managed-diagnostic-pf.elf
# Build the unchanged bounded output frontend; do not alter the ordinary target.
include managed-shell-probe.mk
managed-diagnostic-pf: build/mgmt-guest-continuous.elf

.PHONY: managed-diagnostic-pf-probe
managed-diagnostic-pf-probe: build/krnl-managed-diagnostic-pf.elf build/mgmt-guest-shell-probe.elf
