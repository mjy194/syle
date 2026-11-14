# Experimental Hyper-V reference-counter clock ONLY. No default CPU fallback.
# Requires explicit external QEMU hv-time contract, independently verified.
include managed-baseline.mk
HC_BITS ?= 24
HC_TSC_FREQ ?= 0
ifneq ($(HC_TSC_FREQ),0)
ifneq ($(HC_TSC_FREQ),1)
$(error HC_TSC_FREQ must be exactly 0 or 1)
endif
endif
HC_SUFFIX = $(if $(filter 1,$(HC_TSC_FREQ)),-tsc,)
HC_TAG ?= hc
HC_OUTPUT_TAG ?= host-clock
HC_PREFIX = $(HC_TAG)$(HC_BITS)$(HC_SUFFIX)
HC_DEFS = $(MB_DEFS) -DMG_HOST_CLOCK -DMG_HOST_CLOCK_DIAG -DSYLE_CLOCK_DIAGNOSTIC -DLGS_PREEMPT_BITS=$(HC_BITS) $(if $(filter 1,$(HC_TSC_FREQ)),-DMG_HOST_CLOCK_TSC_FREQ,)
HC_MODS = $(MG_MODS) linux_host_clock linux_host_clock_runtime linux_host_clock_hv linux_host_clock_native $(if $(filter 1,$(HC_TSC_FREQ)),linux_host_tsc_cpuid,)
HC_OBJS = $(addprefix build/$(HC_PREFIX)-,$(addsuffix .o,$(HC_MODS))) build/mb-native-asm.o build/$(MB_EMBED_TAG)-inputs.o build/$(HC_PREFIX)-main.o build/mb-exceptions-asm.o
build/$(HC_PREFIX)-%.o: ../krnl/%.c | build
	$(CC) $(KFLAGS) $(HC_DEFS) -c $< -o $@
build/$(HC_PREFIX)-linux_guest_startup.o: ../krnl/linux_guest_startup.c mb-check-inputs | build
	$(CC) $(KFLAGS) $(HC_DEFS) -DMG_FIXED_INPUTS_VERIFIED -c $< -o $@
build/$(HC_PREFIX)-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(HC_DEFS) -c $< -o $@
build/krnl-managed-$(HC_OUTPUT_TAG)-$(HC_BITS)$(HC_SUFFIX).elf: $(filter-out build/k-exceptions-asm.o build/k-runtime.o,$(KCOMMON)) $(filter-out build/k-ring3.o,$(KRUNTIME)) $(HC_OBJS) build/mt-runtime.o build/mt-ring3.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-host-clock
managed-host-clock: build/krnl-managed-$(HC_OUTPUT_TAG)-$(HC_BITS)$(HC_SUFFIX).elf build/mgmt-guest-terminal.elf
