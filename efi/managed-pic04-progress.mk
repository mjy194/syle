# Additive opt-in: make -f Makefile -f managed-pic04-progress.mk managed-pic04-progress
# Requires the current managed-guest.mk already included by Makefile.
# Never a dependency of all/default; never executes a VM. Private object prefix
# prevents mixing previously compiled baseline startup with legacy PIC flags.
M4D_DEFS = $(MG_DEFS) -DMG_LEGACY_PIC04 -DMG_PROGRESS_DIAG
M4D_MODS = $(sort $(MG_MODS) linux_progress_diag pit0 pic0 pic0_ports pic0_glue pit0_transaction guest_wait legacy_irq_runtime)
M4D_OBJS = $(addprefix build/m4d-,$(addsuffix .o,$(M4D_MODS))) build/m4d-native-asm.o build/mg-inputs.o build/m4d-main.o build/m4d-exceptions-asm.o
build/m4d-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(M4D_DEFS) -c $< -o $@
build/m4d-linux_guest_startup.o: ../krnl/linux_guest_startup.c mg-check-inputs | build
	$(CC) $(KFLAGS) $(M4D_DEFS) -DMG_FIXED_INPUTS_VERIFIED -c $< -o $@
build/m4d-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(M4D_DEFS) -c $< -o $@
build/m4d-exceptions-asm.o: src/linux_exceptions.S | build
	$(CC) -g -fno-pie $(M4D_DEFS) -c $< -o $@
build/m4d-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-managed-pic04-progress.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(M4D_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-pic04-progress
managed-pic04-progress: build/krnl-managed-pic04-progress.elf
