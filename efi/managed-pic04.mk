# Additive opt-in: make -f Makefile -f managed-pic04.mk managed-pic04
# Requires the current managed-guest.mk already included by Makefile.
# Never a dependency of all/default; never executes a VM. Private object prefix
# prevents mixing previously compiled baseline startup with legacy PIC flags.
M4_DEFS = $(MG_DEFS) -DMG_LEGACY_PIC04
M4_MODS = $(sort $(MG_MODS) pit0 pic0 pic0_ports pic0_glue pit0_transaction guest_wait legacy_irq_runtime)
M4_OBJS = $(addprefix build/m4-,$(addsuffix .o,$(M4_MODS))) build/m4-native-asm.o build/mg-inputs.o build/m4-main.o build/m4-exceptions-asm.o
build/m4-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(M4_DEFS) -c $< -o $@
build/m4-linux_guest_startup.o: ../krnl/linux_guest_startup.c mg-check-inputs | build
	$(CC) $(KFLAGS) $(M4_DEFS) -DMG_FIXED_INPUTS_VERIFIED -c $< -o $@
build/m4-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(M4_DEFS) -c $< -o $@
build/m4-exceptions-asm.o: src/linux_exceptions.S | build
	$(CC) -g -fno-pie $(M4_DEFS) -c $< -o $@
build/m4-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-managed-pic04.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(M4_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-pic04
managed-pic04: build/krnl-managed-pic04.elf
