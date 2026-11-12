# Additive opt-in: make -f Makefile -f managed-legacy-progress.mk managed-legacy-progress
# Requires the current managed-guest.mk already included by Makefile.
# Never a dependency of all/default; never executes a VM. Private object prefix
# prevents mixing previously compiled baseline startup with legacy PIC flags.
MD_DEFS = $(MG_DEFS) -DMG_LEGACY_PIC -DMG_PROGRESS_DIAG
MD_MODS = $(sort $(MG_MODS) linux_progress_diag pit0 pic0 pic0_ports pic0_glue pit0_transaction guest_wait legacy_irq_runtime)
MD_OBJS = $(addprefix build/md-,$(addsuffix .o,$(MD_MODS))) build/md-native-asm.o build/mg-inputs.o build/md-main.o build/md-exceptions-asm.o
build/md-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(MD_DEFS) -c $< -o $@
build/md-linux_guest_startup.o: ../krnl/linux_guest_startup.c mg-check-inputs | build
	$(CC) $(KFLAGS) $(MD_DEFS) -DMG_FIXED_INPUTS_VERIFIED -c $< -o $@
build/md-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(MD_DEFS) -c $< -o $@
build/md-exceptions-asm.o: src/linux_exceptions.S | build
	$(CC) -g -fno-pie $(MD_DEFS) -c $< -o $@
build/md-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-managed-legacy-progress.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(MD_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-legacy-progress
managed-legacy-progress: build/krnl-managed-legacy-progress.elf

-include $(MD_OBJS:.o=.d)
