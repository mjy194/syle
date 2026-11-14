# Explicit PIC04 + bounded continuous serial-output frontend. No default change.
# make -f Makefile -f managed-continuous.mk managed-continuous MG_INPUTS=...
include managed-pic04.mk
MC_FLAGS = -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fpie -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables
build/mc-main.o: ../user/mgmt/guest_continuous.c ../user/mgmt/guest_continuous.h ../user/mgmt/guest.h ../include/syle/guest_control.h | build
	$(CC) $(MC_FLAGS) -c $< -o $@
build/mc-entry.o: ../user/mgmt/guest_continuous_entry.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/mgmt-guest-continuous.elf: build/mc-entry.o build/mc-main.o ../user/mgmt/link.ld
	$(LD) -nostdlib -static -T ../user/mgmt/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-continuous
managed-continuous: build/krnl-managed-pic04.elf build/mgmt-guest-continuous.elf
include managed-pic04-progress.mk
.PHONY: managed-continuous-progress
managed-continuous-progress: build/krnl-managed-pic04-progress.elf build/mgmt-guest-continuous.elf
