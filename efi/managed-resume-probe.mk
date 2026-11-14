# Independent same-session STOP/START acceptance probe; defaults unchanged.
include managed-continuous.mk
build/mrp-main.o: ../user/mgmt/guest_resume_probe.c ../user/mgmt/guest_resume_probe.h ../user/mgmt/guest_continuous.h ../user/mgmt/guest.h ../include/syle/guest_control.h | build
	$(CC) $(MC_FLAGS) -c $< -o $@
build/mrp-entry.o: ../user/mgmt/guest_resume_probe_entry.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/mrp-mem.o: ../user/mgmt/freestanding_mem.c | build
	$(CC) $(MC_FLAGS) -c $< -o $@
build/mgmt-guest-resume-probe.elf: build/mrp-entry.o build/mrp-main.o build/mrp-mem.o ../user/mgmt/link.ld
	$(LD) -nostdlib -static -T ../user/mgmt/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-resume-probe
managed-resume-probe: build/krnl-managed-pic04.elf build/mgmt-guest-resume-probe.elf
