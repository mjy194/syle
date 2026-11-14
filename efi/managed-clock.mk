# Explicit A/B target; shared baseline objects are unchanged except the session
# object is replaced at link time. Separate output per bits value avoids stale
# object reuse across comparisons. Original target/profile remains bits=28.
include managed-baseline.mk
CLOCK_BITS ?= 28
CLOCK_SESSION = build/clock-session-$(CLOCK_BITS).o
CLOCK_OBJS = $(filter-out build/mb-linux_guest_session.o,$(MB_OBJS)) $(CLOCK_SESSION)
$(CLOCK_SESSION): ../krnl/linux_guest_session.c ../krnl/linux_clock_slice.h ../krnl/linux_clock_diag.h | build
	$(CC) $(KFLAGS) $(MB_DEFS) -DSYLE_CLOCK_DIAGNOSTIC -DLGS_PREEMPT_BITS=$(CLOCK_BITS) -c $< -o $@
build/krnl-managed-clock-$(CLOCK_BITS).elf: $(filter-out build/k-exceptions-asm.o build/k-runtime.o,$(KCOMMON)) $(filter-out build/k-ring3.o,$(KRUNTIME)) $(CLOCK_OBJS) build/mt-runtime.o build/mt-ring3.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-clock
managed-clock: build/krnl-managed-clock-$(CLOCK_BITS).elf build/mgmt-guest-terminal.elf
