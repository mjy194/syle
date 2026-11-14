# Explicit host-COM1 to guest-UART terminal. Default/probe targets unchanged.
# Builds only; does not launch a VM or change host configuration.
include managed-pic04.mk
MT_FLAGS = -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding -fpie -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables
build/mt-main.o: ../user/mgmt/guest_terminal.c ../user/mgmt/guest_terminal.h ../user/mgmt/guest_continuous.h ../user/mgmt/guest.h ../include/syle/guest_control.h ../include/syle/host_console.h | build
	$(CC) $(MT_FLAGS) -c $< -o $@
build/mt-entry.o: ../user/mgmt/guest_terminal_entry.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/mt-runtime.o: ../krnl/runtime.c ../krnl/host_uart_input.h ../include/syle/host_console.h | build
	$(CC) $(KFLAGS) -DSYLE_HOST_UART_INPUT -c $< -o $@
build/mt-ring3.o: ../krnl/ring3.c ../include/syle/host_console.h ../include/syle/guest_control.h | build
	$(CC) $(KFLAGS) -DSYLE_HOST_UART_INPUT -c $< -o $@
build/mgmt-guest-terminal.elf: build/mt-entry.o build/mt-main.o ../user/mgmt/link.ld
	$(LD) -nostdlib -static -T ../user/mgmt/link.ld $(filter %.o,$^) -o $@
build/krnl-managed-terminal.elf: $(filter-out build/k-exceptions-asm.o build/k-runtime.o,$(KCOMMON)) $(filter-out build/k-ring3.o,$(KRUNTIME)) $(M4_OBJS) build/mt-runtime.o build/mt-ring3.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
.PHONY: managed-terminal
managed-terminal: build/krnl-managed-terminal.elf build/mgmt-guest-terminal.elf
