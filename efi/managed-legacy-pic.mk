# Additive opt-in: make -f Makefile -f managed-legacy-pic.mk managed-legacy-pic
# Requires the current managed-guest.mk already included by Makefile.
# Never a dependency of all/default; never executes a VM. Private object prefix
# prevents mixing previously compiled baseline startup with legacy PIC flags.
ML_DEFS = $(MG_DEFS) -DMG_LEGACY_PIC
ML_MODS = $(sort $(MG_MODS) pit0 pic0 pic0_ports pic0_glue pit0_transaction guest_wait legacy_irq_runtime)
ML_OBJS = $(addprefix build/ml-,$(addsuffix .o,$(ML_MODS))) build/ml-native-asm.o build/mg-inputs.o build/ml-main.o build/ml-exceptions-asm.o
build/ml-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(ML_DEFS) -c $< -o $@
build/ml-linux_guest_startup.o: ../krnl/linux_guest_startup.c mg-check-inputs | build
	$(CC) $(KFLAGS) $(ML_DEFS) -DMG_FIXED_INPUTS_VERIFIED -c $< -o $@
build/ml-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(ML_DEFS) -c $< -o $@
build/ml-exceptions-asm.o: src/linux_exceptions.S | build
	$(CC) -g -fno-pie $(ML_DEFS) -c $< -o $@
build/ml-native-asm.o: ../krnl/linux_vcpu_native.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-managed-legacy-pic.elf: $(filter-out build/k-exceptions-asm.o,$(KCOMMON)) $(KRUNTIME) $(ML_OBJS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-legacy-pic
managed-legacy-pic: build/krnl-managed-legacy-pic.elf
