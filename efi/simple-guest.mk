# Opt-in technical prototype. No dependency from all or other diagnostic targets.
SG_DEFS = -DSIMPLE_GUEST_PROTOTYPE -DSMOKE_TEST
SG_OBJECTS = build/k-simple-main.o build/k-simple-probe.o \
    build/k-simple-guest.o build/k-simple-dispatch.o build/k-simple-image.o \
    build/k-simple-memory.o build/k-simple-entry.o build/k-vmexit.o \
    build/k-simple-io.o build/k-guest-uart.o

build/main-simple-guest.o: src/main.c | build
	$(CC) $(CFLAGS) $(SG_DEFS) -c $< -o $@
build/stage0-simple-guest.so: build/main-simple-guest.o $(BOOT_OBJS)
	$(LD) $(LDFLAGS) $(EFI_LIB)/crt0-efi-x86_64.o $^ -L$(EFI_LIB) -lefi -lgnuefi -o $@
build/k-simple-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) $(SG_DEFS) -c $< -o $@
build/k-simple-probe.o: ../krnl/vmx.c | build
	$(CC) $(KFLAGS) -DVMX_ROOT_TEST -DVMX_GUEST_TEST -DVMX_EPT_TEST -c $< -o $@
build/k-simple-guest.o: ../krnl/simple_guest.c ../krnl/simple_guest.h ../krnl/guest_uart.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-simple-dispatch.o: ../krnl/simple_guest_dispatch.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-simple-image.o: ../krnl/simple_guest_image.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-simple-memory.o: ../krnl/simple_guest_memory.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-simple-entry.o: ../krnl/vmx_guest.S | build
	$(CC) -g -fno-pie -DVMX_PF_TEST -c $< -o $@
build/krnl-simple-guest.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) $(SG_OBJECTS) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/simple-guest-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-simple-guest.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
build/simple-guest.o: ../guest/simple/main.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/simple-guest.elf: build/simple-guest.o ../guest/simple/link.ld
	$(LD) -nostdlib -static -T ../guest/simple/link.ld $< -o $@
build/simple-guest-esp/syle/guest.elf: build/simple-guest.elf
	mkdir -p $(@D)
	cp $< $@
build/simple-guest-esp/syle/boot.cfg: ../boot/boot.cfg
	mkdir -p $(@D)
	cp $< $@
.PHONY: simple-guest
simple-guest: build/simple-guest-esp/EFI/BOOT/BOOTX64.EFI \
    build/simple-guest-esp/syle/krnl.elf build/simple-guest-esp/syle/guest.elf \
    build/simple-guest-esp/syle/boot.cfg

build/k-simple-io.o: ../krnl/simple_guest_io.c ../krnl/simple_guest.h ../krnl/guest_uart.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-guest-uart.o: ../krnl/guest_uart.c ../krnl/guest_uart.h | build
	$(CC) $(KFLAGS) -c $< -o $@

# UART sample is an independent image/ESP; the original sample remains intact.
build/uart-guest.o: ../guest/uart/main.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/uart-guest.elf: build/uart-guest.o ../guest/simple/link.ld
	$(LD) -nostdlib -static -T ../guest/simple/link.ld $< -o $@
build/simple-guest-uart-esp/EFI/BOOT/BOOTX64.EFI: build/simple-guest-esp/EFI/BOOT/BOOTX64.EFI
	mkdir -p $(@D)
	cp $< $@
build/simple-guest-uart-esp/syle/krnl.elf: build/simple-guest-esp/syle/krnl.elf
	mkdir -p $(@D)
	cp $< $@
build/simple-guest-uart-esp/syle/boot.cfg: build/simple-guest-esp/syle/boot.cfg
	mkdir -p $(@D)
	cp $< $@
build/simple-guest-uart-esp/syle/guest.elf: build/uart-guest.elf
	mkdir -p $(@D)
	cp $< $@
.PHONY: simple-guest-uart
simple-guest-uart: build/simple-guest-uart-esp/EFI/BOOT/BOOTX64.EFI \
    build/simple-guest-uart-esp/syle/krnl.elf build/simple-guest-uart-esp/syle/guest.elf \
    build/simple-guest-uart-esp/syle/boot.cfg
