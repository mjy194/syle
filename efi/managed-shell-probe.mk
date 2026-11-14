# Opt-in fixed shell command probe. Does not execute a VM or change defaults.
# make -f Makefile -f managed-shell-probe.mk managed-shell-probe MG_INPUTS=...
include managed-continuous.mk
build/msp-main.o: ../user/mgmt/guest_shell_probe.c ../user/mgmt/guest_shell_probe.h ../user/mgmt/guest_continuous.h ../user/mgmt/guest.h ../include/syle/guest_control.h | build
	$(CC) $(MC_FLAGS) -c $< -o $@
build/msp-entry.o: ../user/mgmt/guest_shell_probe_entry.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/mgmt-guest-shell-probe.elf: build/msp-entry.o build/msp-main.o ../user/mgmt/link.ld
	$(LD) -nostdlib -static -T ../user/mgmt/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-shell-probe managed-shell-probe-progress
managed-shell-probe: build/krnl-managed-pic04.elf build/mgmt-guest-shell-probe.elf
managed-shell-probe-progress: build/krnl-managed-pic04-progress.elf build/mgmt-guest-shell-probe.elf
