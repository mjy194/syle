# Same guest STOP/START plus external paused-duration handshake.
include managed-host-clock.mk
build/mcp-main.o: ../user/mgmt/guest_clock_pause.c ../user/mgmt/guest_resume_probe.h ../user/mgmt/guest_continuous.h ../include/syle/host_console.h | build
	$(CC) $(MT_FLAGS) -c $< -o $@
build/mcp-entry.o: ../user/mgmt/guest_clock_pause_entry.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/mcp-mem.o: ../user/mgmt/freestanding_mem.c | build
	$(CC) $(MT_FLAGS) -c $< -o $@
build/mgmt-guest-clock-pause.elf: build/mcp-entry.o build/mcp-main.o build/mcp-mem.o ../user/mgmt/link.ld
	$(LD) -nostdlib -static -T ../user/mgmt/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-clock-pause
managed-clock-pause: build/krnl-managed-host-clock-$(HC_BITS)$(HC_SUFFIX).elf build/mgmt-guest-clock-pause.elf
