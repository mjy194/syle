# UEFI loader and standalone fixed-address kernel ELF are separate link units.
EFI_PREFIX ?= /usr
EFI_INC ?= $(EFI_PREFIX)/include/efi
EFI_LIB ?= $(EFI_PREFIX)/lib
CC = gcc
LD = ld
OBJCOPY = objcopy
CFLAGS = -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I$(EFI_INC)/protocol -ffreestanding -fpic -fshort-wchar -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-stack-check -fno-asynchronous-unwind-tables -fno-unwind-tables -MMD -MP -Wall -Wextra -Werror -g -O1
KFLAGS = $(filter-out -fpic,$(CFLAGS)) -fno-pic -fno-pie -mcmodel=small -DSYLE_KERNEL
LDFLAGS = -nostdlib -znocombreloc -T $(EFI_LIB)/elf_x86_64_efi.lds -shared -Bsymbolic
SECTIONS = -j .rodata -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc
BOOT_OBJS = build/console.o build/boot-policy.o build/boot-config.o build/kernel-loader.o
KCOMMON = build/k-guest-control.o build/k-vmx-memory.o build/k-vmx.o build/k-vmx-policy.o build/k-memory.o build/k-paging.o build/k-entry.o build/k-runtime.o build/k-console.o build/k-exceptions.o build/k-exceptions-asm.o
KRUNTIME = build/k-ring3.o build/k-user-entry.o build/k-user-elf.o
.PHONY: all smoke faults ring3 clean
all: build/esp/EFI/BOOT/BOOTX64.EFI build/esp/syle/krnl.elf build/esp/syle/mgmt.elf build/esp/syle/boot.cfg
smoke: build/smoke-esp/EFI/BOOT/BOOTX64.EFI build/smoke-esp/syle/krnl.elf
faults: $(foreach n,ud pf,build/$(n)-esp/EFI/BOOT/BOOTX64.EFI build/$(n)-esp/syle/krnl.elf)
ring3: $(foreach n,0 1 2 3 4,build/ring3-$(n)-esp/EFI/BOOT/BOOTX64.EFI build/ring3-$(n)-esp/syle/krnl.elf)

build/main.o: src/main.c | build
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@
build/main-smoke.o: src/main.c | build
	$(CC) $(CFLAGS) -DSMOKE_TEST -c $< -o $@
build/main-ud.o: src/main.c | build
	$(CC) $(CFLAGS) -DFAULT_TEST=6 -c $< -o $@
build/main-pf.o: src/main.c | build
	$(CC) $(CFLAGS) -DFAULT_TEST=14 -c $< -o $@
build/main-ring3.o: src/main.c | build
	$(CC) $(CFLAGS) -DRING3_TEST -c $< -o $@
build/console.o: src/console.c | build
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@
build/boot-policy.o: ../boot/boot.c | build
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@
build/boot-config.o: ../boot/config.c | build
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@
build/kernel-loader.o: ../boot/kernel_elf.c | build
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@
build/stage0.so: build/main.o $(BOOT_OBJS)
	$(LD) $(LDFLAGS) $(EFI_LIB)/crt0-efi-x86_64.o $^ -L$(EFI_LIB) -lefi -lgnuefi -o $@
build/stage0-smoke.so: build/main-smoke.o $(BOOT_OBJS)
	$(LD) $(LDFLAGS) $(EFI_LIB)/crt0-efi-x86_64.o $^ -L$(EFI_LIB) -lefi -lgnuefi -o $@
build/stage0-ud.so: build/main-ud.o $(BOOT_OBJS)
	$(LD) $(LDFLAGS) $(EFI_LIB)/crt0-efi-x86_64.o $^ -L$(EFI_LIB) -lefi -lgnuefi -o $@
build/stage0-pf.so: build/main-pf.o $(BOOT_OBJS)
	$(LD) $(LDFLAGS) $(EFI_LIB)/crt0-efi-x86_64.o $^ -L$(EFI_LIB) -lefi -lgnuefi -o $@
build/stage0-ring3-%.so: build/main-ring3.o $(BOOT_OBJS)
	$(LD) $(LDFLAGS) $(EFI_LIB)/crt0-efi-x86_64.o $^ -L$(EFI_LIB) -lefi -lgnuefi -o $@
build/esp/EFI/BOOT/BOOTX64.EFI: build/stage0.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
build/%-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-%.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@

# Kernel: no EFI libraries, no runtime relocation, entry owns its stack.
build/k-entry.o: ../krnl/entry.S | build
	mkdir -p build
	$(CC) -g -fno-pie -c $< -o $@
build/k-runtime.o: ../krnl/runtime.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-smoke.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DSMOKE_TEST -c $< -o $@
build/k-ud.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DFAULT_TEST=6 -c $< -o $@
build/k-pf.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DFAULT_TEST=14 -c $< -o $@
build/k-ring3-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DRING3_TEST -c $< -o $@
build/k-console.o: src/console.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-exceptions.o: src/exceptions.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-exceptions-asm.o: src/exceptions.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/k-guest-control.o: ../krnl/guest_control.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-ring3.o: ../krnl/ring3.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-ring3-%.o: ../krnl/ring3.c | build
	$(CC) $(KFLAGS) -DRING3_CASE=$* -c $< -o $@
build/k-user-entry.o: ../krnl/arch/x86_64/user.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/k-user-elf.o: ../krnl/user_elf.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/krnl.elf: $(KCOMMON) build/k-main.o $(KRUNTIME) ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/krnl-smoke.elf: $(KCOMMON) build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/krnl-ud.elf: $(KCOMMON) build/k-ud.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/krnl-pf.elf: $(KCOMMON) build/k-pf.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/krnl-ring3-%.elf: $(KCOMMON) build/k-ring3-main.o build/k-ring3-%.o build/k-user-entry.o build/user-image.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/esp/syle/krnl.elf: build/krnl.elf
	mkdir -p $(@D)
	cp $< $@
build/%-esp/syle/krnl.elf: build/krnl-%.elf
	mkdir -p $(@D)
	cp $< $@

build/mgmt.o: ../user/mgmt/main.S | build
	mkdir -p build
	$(CC) -c -nostdlib $< -o $@
build/mgmt.elf: build/mgmt.o
	$(LD) -nostdlib -Ttext=0 -e _start $< -o $@
build/mgmt.bin: build/mgmt.elf
	$(OBJCOPY) -O binary -j .text $< $@
build/user-image.o: ../krnl/arch/x86_64/image.S build/mgmt.bin | build
	$(CC) -c -fno-pie $< -o $@
build/mgmt-runtime.elf: build/mgmt.o ../user/mgmt/link.ld
	$(LD) -nostdlib -T ../user/mgmt/link.ld $< -o $@
build/esp/syle/mgmt.elf: build/mgmt-runtime.elf
	mkdir -p $(@D)
	cp $< $@
build/esp/syle/boot.cfg: ../boot/boot.cfg
	mkdir -p $(@D)
	cp $< $@
clean:
	rm -rf build
build/%.d: ;
-include $(wildcard build/*.d)
.PRECIOUS: build/stage0-%.so build/krnl-ring3-%.elf build/k-ring3-%.o

build:
	mkdir -p build

build/k-memory.o: ../krnl/memory.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-paging.o: ../krnl/paging.c | build
	$(CC) $(KFLAGS) -c $< -o $@

.PHONY: memory-checks
memory-checks: $(foreach n,1 2 3,build/krnl-memory-$(n).elf)
build/k-memory-main-%.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DMEMORY_CASE=$* -DSMOKE_TEST -c $< -o $@
build/krnl-memory-%.elf: $(KCOMMON) build/k-memory-main-%.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@

# Optional VMXON/VMXOFF diagnostic; default build is read-only capability probe.
# Use a separate object to avoid stale compiler-flag dependent builds.
build/k-vmx.o: ../krnl/vmx.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-vmx-policy.o: ../krnl/vmx_policy.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-vmx-root.o: ../krnl/vmx.c | build
	$(CC) $(KFLAGS) -DVMX_ROOT_TEST -c $< -o $@
build/krnl-vmx.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-vmx-root.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx
vmx: build/vmx-esp/EFI/BOOT/BOOTX64.EFI build/vmx-esp/syle/krnl.elf

# Trusted diagnostic guest: deliberately never included in default builds.
build/k-vmx-memory.o: ../krnl/vmx_memory.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-vmx-guest-probe.o: ../krnl/vmx.c | build
	$(CC) $(KFLAGS) -DVMX_ROOT_TEST -DVMX_GUEST_TEST -c $< -o $@
build/k-vmx-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-vmx-guest-entry.o: ../krnl/vmx_guest.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-vmx-guest.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-vmx-guest-probe.o build/k-vmx-guest.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-guest-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-guest
vmx-guest: build/vmx-guest-esp/EFI/BOOT/BOOTX64.EFI build/vmx-guest-esp/syle/krnl.elf

# Separate opt-in EPT diagnostic; normal and shared-page-table guests unchanged.
build/k-ept-probe.o: ../krnl/vmx.c | build
	$(CC) $(KFLAGS) -DVMX_ROOT_TEST -DVMX_GUEST_TEST -DVMX_EPT_TEST -c $< -o $@
build/k-ept-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -c $< -o $@
build/k-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-ept-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-vmx-ept.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-ept-guest.o build/k-ept.o build/k-ept-payload.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-ept-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-ept
vmx-ept: build/vmx-ept-esp/EFI/BOOT/BOOTX64.EFI build/vmx-ept-esp/syle/krnl.elf

# CPUID/HLT diagnostic with independent EPT guest backing.
build/k-exits-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_EXIT_TEST -c $< -o $@
build/k-vmexit.o: ../krnl/vmexit.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-exits-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_EXIT_TEST -c $< -o $@
build/krnl-vmx-exits.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-exits-guest.o build/k-ept.o build/k-exits-payload.o build/k-vmexit.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-exits-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-exits
vmx-exits: build/vmx-exits-esp/EFI/BOOT/BOOTX64.EFI build/vmx-exits-esp/syle/krnl.elf

# Synchronous exception injection and explicit vCPU lifecycle diagnostic.
build/k-events-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_EVENT_TEST -c $< -o $@
build/k-vcpu.o: ../krnl/vcpu.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-events-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EVENT_TEST -c $< -o $@
build/k-events-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_EVENT_TEST -c $< -o $@
build/krnl-vmx-events.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-events-guest.o build/k-events-ept.o build/k-events-payload.o build/k-vcpu.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-events-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-events
vmx-events: build/vmx-events-esp/EFI/BOOT/BOOTX64.EFI build/vmx-events-esp/syle/krnl.elf

# Synchronous exception injection and explicit vCPU lifecycle diagnostic.
build/k-wakeup-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_EVENT_TEST -DVMX_WAKE_TEST -c $< -o $@
build/k-wakeup-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EVENT_TEST -DVMX_WAKE_TEST -c $< -o $@
build/k-wakeup-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_EVENT_TEST -DVMX_WAKE_TEST -c $< -o $@
build/krnl-vmx-wakeup.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-wakeup-guest.o build/k-wakeup-ept.o build/k-wakeup-payload.o build/k-vcpu.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-wakeup-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-wakeup
vmx-wakeup: build/vmx-wakeup-esp/EFI/BOOT/BOOTX64.EFI build/vmx-wakeup-esp/syle/krnl.elf

# Synchronous exception injection and explicit vCPU lifecycle diagnostic.
build/k-gp-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_EVENT_TEST -DVMX_GP_TEST -c $< -o $@
build/k-gp-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EVENT_TEST -DVMX_GP_TEST -c $< -o $@
build/k-gp-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_EVENT_TEST -DVMX_GP_TEST -c $< -o $@
build/krnl-vmx-gp.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-gp-guest.o build/k-gp-ept.o build/k-gp-payload.o build/k-vcpu.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-gp-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-gp
vmx-gp: build/vmx-gp-esp/EFI/BOOT/BOOTX64.EFI build/vmx-gp-esp/syle/krnl.elf


build/k-pf-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_EVENT_TEST -DVMX_PF_TEST -c $< -o $@
build/k-pf-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EVENT_TEST -DVMX_PF_TEST -c $< -o $@
build/k-pf-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_EVENT_TEST -DVMX_PF_TEST -c $< -o $@
build/krnl-vmx-pf.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-pf-guest.o build/k-pf-ept.o build/k-pf-payload.o build/k-vcpu.o build/k-pf-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-pf-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-pf
vmx-pf: build/vmx-pf-esp/EFI/BOOT/BOOTX64.EFI build/vmx-pf-esp/syle/krnl.elf


build/k-pf-entry.o: ../krnl/vmx_guest.S | build
	$(CC) -g -fno-pie -DVMX_PF_TEST -c $< -o $@

build/k-demand-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_EVENT_TEST -DVMX_PF_TEST -DVMX_DEMAND_TEST -c $< -o $@
build/k-demand-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EVENT_TEST -DVMX_PF_TEST -DVMX_DEMAND_TEST -c $< -o $@
build/k-demand-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_EVENT_TEST -DVMX_PF_TEST -DVMX_DEMAND_TEST -c $< -o $@
build/krnl-vmx-demand.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-demand-guest.o build/k-demand-ept.o build/k-demand-payload.o build/k-vcpu.o build/k-pf-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-demand-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-demand
vmx-demand: build/vmx-demand-esp/EFI/BOOT/BOOTX64.EFI build/vmx-demand-esp/syle/krnl.elf




build/k-apic-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_APIC_TEST -c $< -o $@
build/k-apic-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_APIC_TEST -c $< -o $@
build/k-apic-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_APIC_TEST -c $< -o $@
build/k-lapic.o: ../krnl/lapic.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/krnl-vmx-apic.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-apic-guest.o build/k-apic-ept.o build/k-apic-payload.o build/k-lapic.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-apic-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-apic
vmx-apic: build/vmx-apic-esp/EFI/BOOT/BOOTX64.EFI build/vmx-apic-esp/syle/krnl.elf

build/k-window-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_APIC_TEST -DVMX_WINDOW_TEST -c $< -o $@
build/k-window-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_APIC_TEST -DVMX_WINDOW_TEST -c $< -o $@
build/k-window-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_APIC_TEST -DVMX_WINDOW_TEST -c $< -o $@
build/krnl-vmx-window.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-window-guest.o build/k-window-ept.o build/k-window-payload.o build/k-lapic.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-window-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-window
vmx-window: build/vmx-window-esp/EFI/BOOT/BOOTX64.EFI build/vmx-window-esp/syle/krnl.elf

build/k-movss-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_APIC_TEST -DVMX_WINDOW_TEST -DVMX_MOVSS_TEST -c $< -o $@
build/k-movss-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_APIC_TEST -DVMX_WINDOW_TEST -DVMX_MOVSS_TEST -c $< -o $@
build/k-movss-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_APIC_TEST -DVMX_WINDOW_TEST -DVMX_MOVSS_TEST -c $< -o $@
build/krnl-vmx-movss.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-movss-guest.o build/k-movss-ept.o build/k-movss-payload.o build/k-lapic.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-movss-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-movss
vmx-movss: build/vmx-movss-esp/EFI/BOOT/BOOTX64.EFI build/vmx-movss-esp/syle/krnl.elf

build/k-collision-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_APIC_TEST -DVMX_WINDOW_TEST -DVMX_MOVSS_TEST -DVMX_COLLISION_TEST -c $< -o $@
build/k-collision-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_APIC_TEST -DVMX_WINDOW_TEST -DVMX_MOVSS_TEST -DVMX_COLLISION_TEST -c $< -o $@
build/k-collision-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_APIC_TEST -DVMX_WINDOW_TEST -DVMX_MOVSS_TEST -DVMX_COLLISION_TEST -c $< -o $@
build/krnl-vmx-collision.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-collision-guest.o build/k-collision-ept.o build/k-collision-payload.o build/k-lapic.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-collision-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-collision
vmx-collision: build/vmx-collision-esp/EFI/BOOT/BOOTX64.EFI build/vmx-collision-esp/syle/krnl.elf

build/k-collision-deliver-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_APIC_TEST -DVMX_WINDOW_TEST -DVMX_MOVSS_TEST -DVMX_COLLISION_TEST -DVMX_COLLIDE_DELIVER_TEST -c $< -o $@
build/k-collision-deliver-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_APIC_TEST -DVMX_WINDOW_TEST -DVMX_MOVSS_TEST -DVMX_COLLISION_TEST -DVMX_COLLIDE_DELIVER_TEST -c $< -o $@
build/k-collision-deliver-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_APIC_TEST -DVMX_WINDOW_TEST -DVMX_MOVSS_TEST -DVMX_COLLISION_TEST -DVMX_COLLIDE_DELIVER_TEST -c $< -o $@
build/krnl-vmx-collision-deliver.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-collision-deliver-guest.o build/k-collision-deliver-ept.o build/k-collision-deliver-payload.o build/k-lapic.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-collision-deliver-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-collision-deliver
vmx-collision-deliver: build/vmx-collision-deliver-esp/EFI/BOOT/BOOTX64.EFI build/vmx-collision-deliver-esp/syle/krnl.elf

build/k-vectoring-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_EVENT_TEST -DVMX_GP_TEST -DVMX_VECTORING_TEST -c $< -o $@
build/k-vectoring-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EVENT_TEST -DVMX_GP_TEST -DVMX_VECTORING_TEST -c $< -o $@
build/k-vectoring-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_EVENT_TEST -DVMX_GP_TEST -DVMX_VECTORING_TEST -c $< -o $@
build/krnl-vmx-vectoring.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-vectoring-guest.o build/k-vectoring-ept.o build/k-vectoring-payload.o build/k-vcpu.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-vectoring-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-vectoring
vmx-vectoring: build/vmx-vectoring-esp/EFI/BOOT/BOOTX64.EFI build/vmx-vectoring-esp/syle/krnl.elf



build/k-stackpart-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_EVENT_TEST -DVMX_GP_TEST -DVMX_VECTORING_TEST -DVMX_STACKPART_TEST -c $< -o $@
build/k-stackpart-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EVENT_TEST -DVMX_GP_TEST -DVMX_VECTORING_TEST -DVMX_STACKPART_TEST -c $< -o $@
build/k-stackpart-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_EVENT_TEST -DVMX_GP_TEST -DVMX_VECTORING_TEST -DVMX_STACKPART_TEST -c $< -o $@
build/krnl-vmx-stackpart.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-stackpart-guest.o build/k-stackpart-ept.o build/k-stackpart-payload.o build/k-vcpu.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-stackpart-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-stackpart
vmx-stackpart: build/vmx-stackpart-esp/EFI/BOOT/BOOTX64.EFI build/vmx-stackpart-esp/syle/krnl.elf



build/k-timer-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_APIC_TEST -DVMX_TIMER_TEST -c $< -o $@
build/k-timer-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_APIC_TEST -DVMX_TIMER_TEST -c $< -o $@
build/k-timer-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_APIC_TEST -DVMX_TIMER_TEST -c $< -o $@
build/krnl-vmx-timer.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-timer-guest.o build/k-timer-ept.o build/k-timer-payload.o build/k-lapic.o build/k-lapic-timer.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-timer-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-timer
vmx-timer: build/vmx-timer-esp/EFI/BOOT/BOOTX64.EFI build/vmx-timer-esp/syle/krnl.elf


build/k-lapic-timer.o: ../krnl/lapic_timer.c | build
	$(CC) $(KFLAGS) -c $< -o $@

build/k-periodic-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_APIC_TEST -DVMX_TIMER_TEST -DVMX_PERIODIC_TEST -c $< -o $@
build/k-periodic-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_APIC_TEST -DVMX_TIMER_TEST -DVMX_PERIODIC_TEST -c $< -o $@
build/k-periodic-payload.o: ../krnl/ept_payload.S | build
	$(CC) -g -fno-pie -DVMX_APIC_TEST -DVMX_TIMER_TEST -DVMX_PERIODIC_TEST -c $< -o $@
build/krnl-vmx-periodic.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-periodic-guest.o build/k-periodic-ept.o build/k-periodic-payload.o build/k-lapic.o build/k-lapic-timer.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-periodic-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-periodic
vmx-periodic: build/vmx-periodic-esp/EFI/BOOT/BOOTX64.EFI build/vmx-periodic-esp/syle/krnl.elf



# Separate device/MMIO vertical diagnostic, preserving prior images.
build/k-device_bus.o: ../krnl/device_bus.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-ioapic.o: ../krnl/ioapic.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-mmio_decode.o: ../krnl/mmio_decode.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-device_guest.o: ../krnl/device_guest.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-ioapic-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_APIC_TEST -DVMX_IOAPIC_TEST -c $< -o $@
build/k-ioapic-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_APIC_TEST -DVMX_IOAPIC_TEST -c $< -o $@
build/k-ioapic-payload.o: ../krnl/ept_payload.S ../krnl/device_payload.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_APIC_TEST -DVMX_IOAPIC_TEST -c $< -o $@
build/krnl-vmx-ioapic.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-ioapic-guest.o build/k-ioapic-ept.o build/k-ioapic-payload.o build/k-lapic.o build/k-device_bus.o build/k-ioapic.o build/k-mmio_decode.o build/k-device_guest.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-ioapic-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-ioapic
vmx-ioapic: build/vmx-ioapic-esp/EFI/BOOT/BOOTX64.EFI build/vmx-ioapic-esp/syle/krnl.elf

# Restricted true L1/L2 nested diagnostic.
build/k-nested_vmx.o: ../krnl/nested_vmx.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -c $< -o $@
build/k-nested_operand.o: ../krnl/nested_operand.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -c $< -o $@
build/k-nested_profile.o: ../krnl/nested_profile.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -c $< -o $@
build/k-nested_guest.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -c $< -o $@
build/k-nested-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -c $< -o $@
build/k-nested-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -c $< -o $@
build/k-nested-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -c $< -o $@
build/krnl-vmx-nested.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested-guest.o build/k-nested-ept.o build/k-nested-payload.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_guest.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested
vmx-nested: build/vmx-nested-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-esp/syle/krnl.elf

# N2 owns separate composed EPT tables; N1 image is unchanged.
build/k-nested_ept.o: ../krnl/nested_ept.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -c $< -o $@
build/k-nested_ept_hw.o: ../krnl/nested_ept_hw.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -c $< -o $@
build/k-nested2-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -c $< -o $@
build/k-nested2-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -c $< -o $@
build/k-nested2-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -c $< -o $@
build/k-nested2-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -c $< -o $@
build/krnl-vmx-nested-ept.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested2-guest.o build/k-nested2-ept.o build/k-nested2-payload.o build/k-nested2-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested_ept_hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-ept-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-ept
vmx-nested-ept: build/vmx-nested-ept-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-ept-esp/syle/krnl.elf

# N3 guest-state capture/restore diagnostic.
build/k-nested3-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_STATE_TEST -c $< -o $@
build/k-nested3-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_STATE_TEST -c $< -o $@
build/k-nested3-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_STATE_TEST -c $< -o $@
build/k-nested3-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_state_l1.inc ../krnl/nested_state_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_STATE_TEST -c $< -o $@
build/krnl-vmx-nested-state.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested3-guest.o build/k-nested3-ept.o build/k-nested3-payload.o build/k-nested3-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-state-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-state
vmx-nested-state: build/vmx-nested-state-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-state-esp/syle/krnl.elf

# N4 controlled early and late virtual entry failures.
build/k-nested4-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_FAILURE_TEST -c $< -o $@
build/k-nested4-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_FAILURE_TEST -c $< -o $@
build/k-nested4-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_FAILURE_TEST -c $< -o $@
build/k-nested4-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_failure_attempt.inc ../krnl/nested_failure_handler.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_FAILURE_TEST -c $< -o $@
build/krnl-vmx-nested-failure.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested4-guest.o build/k-nested4-ept.o build/k-nested4-payload.o build/k-nested4-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-failure-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-failure
vmx-nested-failure: build/vmx-nested-failure-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-failure-esp/syle/krnl.elf

# N5 exception ownership policy and independent roundtrip.
build/k-nested_events.o: ../krnl/nested_events.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested5-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -c $< -o $@
build/k-nested5-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -c $< -o $@
build/k-nested5-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -c $< -o $@
build/k-nested5-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_event_l1.inc ../krnl/nested_event_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -c $< -o $@
build/krnl-vmx-nested-events.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested5-guest.o build/k-nested5-ept.o build/k-nested5-payload.o build/k-nested5-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_events.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-events-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-events
vmx-nested-events: build/vmx-nested-events-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-events-esp/syle/krnl.elf

# N6 L1 requested hardware exception injection.
build/k-nested6-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -c $< -o $@
build/k-nested6-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -c $< -o $@
build/k-nested6-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -c $< -o $@
build/k-nested6-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_event_l1.inc ../krnl/nested_event_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -c $< -o $@
build/krnl-vmx-nested-inject.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested6-guest.o build/k-nested6-ept.o build/k-nested6-payload.o build/k-nested6-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_events.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-inject-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-inject
vmx-nested-inject: build/vmx-nested-inject-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-inject-esp/syle/krnl.elf

# N7 real nonzero-error GP injection.
build/k-nested7-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -c $< -o $@
build/k-nested7-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -c $< -o $@
build/k-nested7-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -c $< -o $@
build/k-nested7-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_event_l1.inc ../krnl/nested_event_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -c $< -o $@
build/krnl-vmx-nested-gp-inject.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested7-guest.o build/k-nested7-ept.o build/k-nested7-payload.o build/k-nested7-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_events.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-gp-inject-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-gp-inject
vmx-nested-gp-inject: build/vmx-nested-gp-inject-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-gp-inject-esp/syle/krnl.elf

build/k-nested8-entry.o: ../krnl/vmx_guest.S | build
	$(CC) -g -fno-pie -DVMX_NESTED_PF_INJECT_TEST -c $< -o $@

# N8 shared architectural CR2 and real PF injection.
build/k-nested8-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -DVMX_NESTED_PF_INJECT_TEST -c $< -o $@
build/k-nested8-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -DVMX_NESTED_PF_INJECT_TEST -c $< -o $@
build/k-nested8-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -DVMX_NESTED_PF_INJECT_TEST -c $< -o $@
build/k-nested8-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_event_l1.inc ../krnl/nested_event_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -DVMX_NESTED_PF_INJECT_TEST -c $< -o $@
build/krnl-vmx-nested-pf-inject.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested8-guest.o build/k-nested8-ept.o build/k-nested8-payload.o build/k-nested8-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_events.o build/k-nested8-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-pf-inject-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-pf-inject
vmx-nested-pf-inject: build/vmx-nested-pf-inject-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-pf-inject-esp/syle/krnl.elf

# N9 real PF mask/match ownership matrix.
build/k-nested9-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -DVMX_NESTED_PF_INJECT_TEST -DVMX_NESTED_PF_ROUTE_TEST -c $< -o $@
build/k-nested9-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -DVMX_NESTED_PF_INJECT_TEST -DVMX_NESTED_PF_ROUTE_TEST -c $< -o $@
build/k-nested9-adapter.o: ../krnl/nested_guest.c ../krnl/nested_pf_route_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -DVMX_NESTED_PF_INJECT_TEST -DVMX_NESTED_PF_ROUTE_TEST -c $< -o $@
build/k-nested9-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_event_l1.inc ../krnl/nested_event_l2.inc ../krnl/nested_pf_route_l1.inc ../krnl/nested_pf_route_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -DVMX_NESTED_PF_INJECT_TEST -DVMX_NESTED_PF_ROUTE_TEST -c $< -o $@
build/krnl-vmx-nested-pf-route.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested9-guest.o build/k-nested9-ept.o build/k-nested9-payload.o build/k-nested9-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_events.o build/k-nested8-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-pf-route-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-pf-route
vmx-nested-pf-route: build/vmx-nested-pf-route-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-pf-route-esp/syle/krnl.elf

# N10 bounded EPT map lifecycle.
build/k-nested_ept_map.o: ../krnl/nested_ept_map.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested_ept_map_hw.o: ../krnl/nested_ept_map_hw.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -c $< -o $@
build/k-nested10-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_MAP_TEST -c $< -o $@
build/k-nested10-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_MAP_TEST -c $< -o $@
build/k-nested10-adapter.o: ../krnl/nested_guest.c ../krnl/nested_ept_map_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_MAP_TEST -c $< -o $@
build/k-nested10-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_ept_map_init.inc ../krnl/nested_ept_map_l1.inc ../krnl/nested_ept_map_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_MAP_TEST -c $< -o $@
build/krnl-vmx-nested-ept-map.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested10-guest.o build/k-nested10-ept.o build/k-nested10-payload.o build/k-nested10-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested_ept_map.o build/k-nested_ept_map_hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-ept-map-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-ept-map
vmx-nested-ept-map: build/vmx-nested-ept-map-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-ept-map-esp/syle/krnl.elf


# N11 sparse four-level EPT02.
build/k-nested_ept_sparse.o: ../krnl/nested_ept_sparse.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested_ept_sparse_hw.o: ../krnl/nested_ept_sparse_hw.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -c $< -o $@
build/k-nested11-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -c $< -o $@
build/k-nested11-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -c $< -o $@
build/k-nested11-adapter.o: ../krnl/nested_guest.c ../krnl/nested_ept_sparse_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -c $< -o $@
build/k-nested11-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_ept_sparse_init.inc ../krnl/nested_ept_sparse_l1.inc ../krnl/nested_ept_sparse_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -c $< -o $@
build/krnl-vmx-nested-ept-sparse.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested11-guest.o build/k-nested11-ept.o build/k-nested11-payload.o build/k-nested11-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_sparse_hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-ept-sparse-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-ept-sparse
vmx-nested-ept-sparse: build/vmx-nested-ept-sparse-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-ept-sparse-esp/syle/krnl.elf


# N12 independent sparse EPT contexts.
build/k-nested_ept_contexts.o: ../krnl/nested_ept_contexts.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested12-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -c $< -o $@
build/k-nested12-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -c $< -o $@
build/k-nested12-adapter.o: ../krnl/nested_guest.c ../krnl/nested_ept_context_adapter.inc ../krnl/nested_ept_context_invept.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -c $< -o $@
build/k-nested12-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_ept_context_init.inc ../krnl/nested_ept_context_l1.inc ../krnl/nested_ept_context_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -c $< -o $@
build/krnl-vmx-nested-ept-context.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested12-guest.o build/k-nested12-ept.o build/k-nested12-payload.o build/k-nested12-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse_hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-ept-context-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-ept-context
vmx-nested-ept-context: build/vmx-nested-ept-context-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-ept-context-esp/syle/krnl.elf


# N13 dynamic registry; hardware resolver uses the expanded private fixture.
build/k-nested13-hw.o: ../krnl/nested_ept_sparse_hw.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_DYNAMIC_TEST -c $< -o $@
build/k-nested13-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_DYNAMIC_TEST -c $< -o $@
build/k-nested13-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_DYNAMIC_TEST -c $< -o $@
build/k-nested13-adapter.o: ../krnl/nested_guest.c ../krnl/nested_ept_dynamic_adapter.inc ../krnl/nested_ept_context_invept.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_DYNAMIC_TEST -c $< -o $@
build/k-nested13-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_ept_dynamic_init.inc ../krnl/nested_ept_dynamic_l1.inc ../krnl/nested_ept_dynamic_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_DYNAMIC_TEST -c $< -o $@
build/krnl-vmx-nested-ept-dynamic.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested13-guest.o build/k-nested13-ept.o build/k-nested13-payload.o build/k-nested13-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_contexts.o build/k-nested13-hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-ept-dynamic-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-ept-dynamic
vmx-nested-ept-dynamic: build/vmx-nested-ept-dynamic-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-ept-dynamic-esp/syle/krnl.elf


# N14 EPT12 large leaves, physical EPT02 remains 4KiB.
build/k-nested14-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_LARGE_TEST -c $< -o $@
build/k-nested14-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_LARGE_TEST -c $< -o $@
build/k-nested14-adapter.o: ../krnl/nested_guest.c ../krnl/nested_ept_large_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_LARGE_TEST -c $< -o $@
build/k-nested14-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_ept_large_init.inc ../krnl/nested_ept_large_l1.inc ../krnl/nested_ept_large_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_LARGE_TEST -c $< -o $@
build/krnl-vmx-nested-ept-large.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested14-guest.o build/k-nested14-ept.o build/k-nested14-payload.o build/k-nested14-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_sparse_hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-ept-large-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-ept-large
vmx-nested-ept-large: build/vmx-nested-ept-large-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-ept-large-esp/syle/krnl.elf



# N15 software EPT12 A/D over physical AD-enabled EPT02.
build/k-nested_ept_ad.o: ../krnl/nested_ept_ad.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested_ept_ad_hw.o: ../krnl/nested_ept_ad_hw.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_AD_TEST -c $< -o $@
build/k-nested15-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_AD_TEST -c $< -o $@
build/k-nested15-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_AD_TEST -c $< -o $@
build/k-nested15-adapter.o: ../krnl/nested_guest.c ../krnl/nested_ept_ad_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_AD_TEST -c $< -o $@
build/k-nested15-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_ept_ad_init.inc ../krnl/nested_ept_ad_l1.inc ../krnl/nested_ept_ad_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_AD_TEST -c $< -o $@
build/krnl-vmx-nested-ept-ad.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested15-guest.o build/k-nested15-ept.o build/k-nested15-payload.o build/k-nested15-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_sparse_hw.o build/k-nested_ept_ad.o build/k-nested_ept_ad_hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-ept-ad-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-ept-ad
vmx-nested-ept-ad: build/vmx-nested-ept-ad-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-ept-ad-esp/syle/krnl.elf



# N16-A permitted failed-walk A side effects.
build/k-nested16-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_FAIL_TEST -c $< -o $@
build/k-nested16-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_FAIL_TEST -c $< -o $@
build/k-nested16-adapter.o: ../krnl/nested_guest.c ../krnl/nested_ept_ad_fail_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_FAIL_TEST -c $< -o $@
build/k-nested16-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_ept_ad_fail_init.inc ../krnl/nested_ept_ad_fail_l1.inc ../krnl/nested_ept_ad_fail_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_FAIL_TEST -c $< -o $@
build/krnl-vmx-nested-ept-ad-fail.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested16-guest.o build/k-nested16-ept.o build/k-nested16-payload.o build/k-nested16-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_sparse_hw.o build/k-nested_ept_ad.o build/k-nested_ept_ad_hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-ept-ad-fail-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-ept-ad-fail
vmx-nested-ept-ad-fail: build/vmx-nested-ept-ad-fail-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-ept-ad-fail-esp/syle/krnl.elf



# N16-B context-owned A/D trace lifetimes.
build/k-nested_ept_ad_context_hw.o: ../krnl/nested_ept_ad_context_hw.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -c $< -o $@
build/k-nested16b-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_CONTEXT_TEST -c $< -o $@
build/k-nested16b-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_CONTEXT_TEST -c $< -o $@
build/k-nested16b-adapter.o: ../krnl/nested_guest.c ../krnl/nested_ept_ad_context_adapter.inc ../krnl/nested_ept_context_invept.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_CONTEXT_TEST -c $< -o $@
build/k-nested16b-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_ept_ad_context_init.inc ../krnl/nested_ept_ad_context_l1.inc ../krnl/nested_ept_ad_context_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_CONTEXT_TEST -c $< -o $@
build/krnl-vmx-nested-ept-ad-context.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested16b-guest.o build/k-nested16b-ept.o build/k-nested16b-payload.o build/k-nested16b-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse_hw.o build/k-nested_ept_ad.o build/k-nested_ept_ad_hw.o build/k-nested_ept_ad_context_hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-ept-ad-context-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-ept-ad-context
vmx-nested-ept-ad-context: build/vmx-nested-ept-ad-context-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-ept-ad-context-esp/syle/krnl.elf



# N16-C explicit same-root A/D mode transitions.
build/k-nested16c-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_CONTEXT_TEST -DVMX_NESTED_EPT_AD_MODE_TEST -c $< -o $@
build/k-nested16c-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_CONTEXT_TEST -DVMX_NESTED_EPT_AD_MODE_TEST -c $< -o $@
build/k-nested16c-adapter.o: ../krnl/nested_guest.c ../krnl/nested_ept_ad_mode_adapter.inc ../krnl/nested_ept_context_invept.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_CONTEXT_TEST -DVMX_NESTED_EPT_AD_MODE_TEST -c $< -o $@
build/k-nested16c-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_ept_ad_mode_init.inc ../krnl/nested_ept_ad_mode_l1.inc ../krnl/nested_ept_ad_mode_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -DVMX_NESTED_EPT_AD_TEST -DVMX_NESTED_EPT_AD_CONTEXT_TEST -DVMX_NESTED_EPT_AD_MODE_TEST -c $< -o $@
build/krnl-vmx-nested-ept-ad-mode.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested16c-guest.o build/k-nested16c-ept.o build/k-nested16c-payload.o build/k-nested16c-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse_hw.o build/k-nested_ept_ad.o build/k-nested_ept_ad_hw.o build/k-nested_ept_ad_context_hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-ept-ad-mode-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-ept-ad-mode
vmx-nested-ept-ad-mode: build/vmx-nested-ept-ad-mode-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-ept-ad-mode-esp/syle/krnl.elf

# N17 memory-type model and WB/IPAT hardware profile.
build/k-nested_memory_type.o: ../krnl/nested_memory_type.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested17-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -c $< -o $@
build/k-nested17-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -c $< -o $@
build/k-nested17-adapter.o: ../krnl/nested_guest.c ../krnl/nested_memory_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -c $< -o $@
build/k-nested17-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_memory_init.inc ../krnl/nested_memory_l1.inc ../krnl/nested_memory_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -c $< -o $@
build/krnl-vmx-nested-memory.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested17-guest.o build/k-nested17-ept.o build/k-nested17-payload.o build/k-nested17-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_memory_type.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_sparse_hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-memory-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-memory
vmx-nested-memory: build/vmx-nested-memory-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-memory-esp/syle/krnl.elf

# N18 isolated two-CPU INIT/SIPI/mailbox diagnostic; no AP firmware callbacks.
build/main-smp.o: src/main.c | build
	$(CC) $(CFLAGS) -DSMOKE_TEST -DSMP_TEST -c $< -o $@
build/stage0-smp.so: build/main-smp.o $(BOOT_OBJS)
	$(LD) $(LDFLAGS) $(EFI_LIB)/crt0-efi-x86_64.o $^ -L$(EFI_LIB) -lefi -lgnuefi -o $@
build/k-smp-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DSMOKE_TEST -DSMP_TEST -c $< -o $@
build/k-smp.o: ../krnl/smp.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-smp-acpi.o: ../krnl/smp_acpi.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-smp-cpu.o: ../krnl/smp_cpu.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-smp-entry.o: ../krnl/smp_entry.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-smp.elf: $(KCOMMON) build/k-smp-main.o build/k-smp.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp
smp: build/smp-esp/EFI/BOOT/BOOTX64.EFI build/smp-esp/syle/krnl.elf

# N19 AP interrupt-driven mailbox (not polling emulation).
build/k-smp-ipi.o: ../krnl/smp.c | build
	$(CC) $(KFLAGS) -DSMP_IPI_TEST -c $< -o $@
build/krnl-smp-ipi.elf: $(KCOMMON) build/k-smp-main.o build/k-smp-ipi.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-ipi-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-ipi
smp-ipi: build/smp-ipi-esp/EFI/BOOT/BOOTX64.EFI build/smp-ipi-esp/syle/krnl.elf

# N20 independent CPU-owned VMXON contexts coordinated with real IPIs.
build/k-smp-vmx-probe.o: ../krnl/smp.c | build
	$(CC) $(KFLAGS) -DSMP_IPI_TEST -DSMP_VMX_TEST -c $< -o $@
build/k-smp-vmx.o: ../krnl/smp_vmx.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/krnl-smp-vmx.elf: $(KCOMMON) build/k-smp-main.o build/k-smp-vmx-probe.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-vmx-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-vmx
smp-vmx: build/smp-vmx-esp/EFI/BOOT/BOOTX64.EFI build/smp-vmx-esp/syle/krnl.elf

# N21 independent per-CPU VMCS/frame and shared immutable EPT guest reads.
build/k-smp-guest-probe.o: ../krnl/smp.c | build
	$(CC) $(KFLAGS) -DSMP_IPI_TEST -DSMP_VMX_TEST -DSMP_GUEST_TEST -c $< -o $@
build/k-smp-guest.o: ../krnl/smp_guest.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-smp-ept.o: ../krnl/smp_ept.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-smp-guest-entry.o: ../krnl/smp_guest_entry.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/krnl-smp-guest.elf: $(KCOMMON) build/k-smp-main.o build/k-smp-guest-probe.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-smp-guest.o build/k-smp-ept.o build/k-smp-guest-entry.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-guest-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-guest
smp-guest: build/smp-guest-esp/EFI/BOOT/BOOTX64.EFI build/smp-guest-esp/syle/krnl.elf

# N22 retained per-CPU VMCS/frame and shared immutable EPT guest reads.
build/k-smp-shootdown-probe.o: ../krnl/smp.c | build
	$(CC) $(KFLAGS) -DSMP_IPI_TEST -DSMP_VMX_TEST -DSMP_GUEST_TEST -DSMP_SHOOTDOWN_TEST -c $< -o $@
build/krnl-smp-shootdown.elf: $(KCOMMON) build/k-smp-main.o build/k-smp-shootdown-probe.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-smp-guest.o build/k-smp-ept.o build/k-smp-guest-entry.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-shootdown-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-shootdown
smp-shootdown: build/smp-shootdown-esp/EFI/BOOT/BOOTX64.EFI build/smp-shootdown-esp/syle/krnl.elf

build/k-shootdown.o: ../krnl/shootdown.c | build
	$(CC) $(KFLAGS) -c $< -o $@
# N23 retained per-CPU VMCS/frame and shared immutable EPT guest reads.
build/k-smp-controller-probe.o: ../krnl/smp.c | build
	$(CC) $(KFLAGS) -DSMP_IPI_TEST -DSMP_VMX_TEST -DSMP_GUEST_TEST -DSMP_CONTROLLER_TEST -c $< -o $@
build/krnl-smp-controller.elf: $(KCOMMON) build/k-smp-main.o build/k-smp-controller-probe.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-shootdown.o build/k-smp-guest.o build/k-smp-ept.o build/k-smp-guest-entry.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-controller-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-controller
smp-controller: build/smp-controller-esp/EFI/BOOT/BOOTX64.EFI build/smp-controller-esp/syle/krnl.elf

# N24 retained per-CPU VMCS/frame and shared immutable EPT guest reads.
build/k-smp-kick-probe.o: ../krnl/smp.c | build
	$(CC) $(KFLAGS) -DSMP_IPI_TEST -DSMP_VMX_TEST -DSMP_GUEST_TEST -DSMP_CONTROLLER_TEST -DSMP_KICK_TEST -c $< -o $@
build/krnl-smp-kick.elf: $(KCOMMON) build/k-smp-main.o build/k-smp-kick-probe.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-shootdown.o build/k-smp-guest.o build/k-smp-ept.o build/k-smp-guest-entry.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-kick-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-kick
smp-kick: build/smp-kick-esp/EFI/BOOT/BOOTX64.EFI build/smp-kick-esp/syle/krnl.elf

# N25 EPT12 large leaves, physical EPT02 remains 4KiB.
build/k-nested25-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_LARGE_TEST -DVMX_NESTED_EPT_MISCONFIG_TEST -c $< -o $@
build/k-nested25-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_LARGE_TEST -DVMX_NESTED_EPT_MISCONFIG_TEST -c $< -o $@
build/k-nested25-adapter.o: ../krnl/nested_guest.c ../krnl/nested_ept_misconfig_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_LARGE_TEST -DVMX_NESTED_EPT_MISCONFIG_TEST -c $< -o $@
build/k-nested25-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_ept_misconfig_init.inc ../krnl/nested_ept_misconfig_l1.inc ../krnl/nested_ept_misconfig_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_EPT_LARGE_TEST -DVMX_NESTED_EPT_MISCONFIG_TEST -c $< -o $@
build/krnl-vmx-nested-ept-misconfig.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested25-guest.o build/k-nested25-ept.o build/k-nested25-payload.o build/k-nested25-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_sparse_hw.o build/k-vmx-guest-entry.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-ept-misconfig-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-ept-misconfig
vmx-nested-ept-misconfig: build/vmx-nested-ept-misconfig-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-ept-misconfig-esp/syle/krnl.elf

# N26 dedicated single-CPU cache-type transition and real UC guest data.
build/k-cache-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DSMOKE_TEST -DCACHE_UC_TEST -c $< -o $@
build/k-cache-page.o: ../krnl/cache_page.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-cache-uc.o: ../krnl/cache_uc_probe.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/krnl-cache-uc.elf: $(KCOMMON) build/k-cache-main.o build/k-cache-page.o build/k-cache-uc.o build/k-smp-acpi.o build/k-smp-vmx.o build/k-smp-guest.o build/k-smp-ept.o build/k-smp-guest-entry.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/cache-uc-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: cache-uc
cache-uc: build/cache-uc-esp/EFI/BOOT/BOOTX64.EFI build/cache-uc-esp/syle/krnl.elf

build/k-nested-uc-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DSMOKE_TEST -DNESTED_UC_PLATFORM_TEST -c $< -o $@
build/k-cache-registered.o: ../krnl/cache_registered.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -c $< -o $@
build/k-nested27-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -c $< -o $@
build/k-nested27-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -c $< -o $@
build/k-nested27-adapter.o: ../krnl/nested_guest.c ../krnl/nested_uc_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -c $< -o $@
build/k-nested27-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_uc_init.inc ../krnl/nested_uc_l1.inc ../krnl/nested_uc_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -c $< -o $@
build/krnl-vmx-nested-uc.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested27-guest.o build/k-nested27-ept.o build/k-nested27-payload.o build/k-nested27-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_memory_type.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_sparse_hw.o build/k-vmx-guest-entry.o build/k-nested-uc-main.o build/k-cache-registered.o build/k-cache-lease-refs.o build/k-cache-page.o build/k-smp-acpi.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-uc-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-uc
vmx-nested-uc: build/vmx-nested-uc-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-uc-esp/syle/krnl.elf

build/k-cache-lease-refs.o: ../krnl/cache_lease_refs.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested28-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_UC_CONTEXT_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -c $< -o $@
build/k-nested28-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_UC_CONTEXT_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -c $< -o $@
build/k-nested28-adapter.o: ../krnl/nested_guest.c ../krnl/nested_uc_context_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_UC_CONTEXT_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -c $< -o $@
build/k-nested28-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_uc_context_init.inc ../krnl/nested_uc_context_l1.inc ../krnl/nested_uc_context_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_UC_CONTEXT_TEST -DVMX_NESTED_EPT_CONTEXT_TEST -c $< -o $@
build/krnl-vmx-nested-uc-context.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested28-guest.o build/k-nested28-ept.o build/k-nested28-payload.o build/k-nested28-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_memory_type.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_sparse_hw.o build/k-nested_ept_contexts.o build/k-vmx-guest-entry.o build/k-nested-uc-main.o build/k-cache-registered.o build/k-cache-lease-refs.o build/k-cache-page.o build/k-smp-acpi.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-uc-context-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-uc-context
vmx-nested-uc-context: build/vmx-nested-uc-context-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-uc-context-esp/syle/krnl.elf

build/k-nested29-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_UC_PAT_TEST -c $< -o $@
build/k-nested29-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_UC_PAT_TEST -c $< -o $@
build/k-nested29-adapter.o: ../krnl/nested_guest.c ../krnl/nested_uc_pat_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_UC_PAT_TEST -c $< -o $@
build/k-nested29-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_uc_pat_init.inc ../krnl/nested_uc_pat_l1.inc ../krnl/nested_uc_pat_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_UC_PAT_TEST -c $< -o $@
build/krnl-vmx-nested-uc-pat.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested29-guest.o build/k-nested29-ept.o build/k-nested29-payload.o build/k-nested29-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_memory_type.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_sparse_hw.o build/k-vmx-guest-entry.o build/k-nested-uc-main.o build/k-cache-registered.o build/k-cache-lease-refs.o build/k-cache-page.o build/k-smp-acpi.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-uc-pat-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-uc-pat
vmx-nested-uc-pat: build/vmx-nested-uc-pat-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-uc-pat-esp/syle/krnl.elf


build/k-nested-wt-main.o: ../krnl/main.c | build
	$(CC) $(KFLAGS) -DSMOKE_TEST -DNESTED_WT_PLATFORM_TEST -c $< -o $@
build/k-nested30-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_WT_TEST -c $< -o $@
build/k-nested30-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_WT_TEST -c $< -o $@
build/k-nested30-adapter.o: ../krnl/nested_guest.c ../krnl/nested_uc_adapter.inc | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_WT_TEST -c $< -o $@
build/k-nested30-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_uc_init.inc ../krnl/nested_uc_l1.inc ../krnl/nested_uc_l2.inc | build
	$(CC) -g -fno-pie -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_EPT_TEST -DVMX_NESTED_EPT_SPARSE_TEST -DVMX_NESTED_MEMORY_TEST -DVMX_NESTED_UC_TEST -DVMX_NESTED_WT_TEST -c $< -o $@
build/krnl-vmx-nested-wt.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-ept-probe.o build/k-nested30-guest.o build/k-nested30-ept.o build/k-nested30-payload.o build/k-nested30-adapter.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_memory_type.o build/k-nested_ept.o build/k-nested_ept_sparse.o build/k-nested_ept_sparse_hw.o build/k-vmx-guest-entry.o build/k-nested-wt-main.o build/k-cache-registered.o build/k-cache-lease-refs.o build/k-cache-page.o build/k-smp-acpi.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-wt-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-wt
vmx-nested-wt: build/vmx-nested-wt-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-wt-esp/syle/krnl.elf


build/k-cache-rendezvous.o: ../krnl/cache_rendezvous.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-cache-maintenance.o: ../krnl/cache_maintenance.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-smp-maintenance-probe.o: ../krnl/smp.c | build
	$(CC) $(KFLAGS) -DSMP_IPI_TEST -DSMP_VMX_TEST -DSMP_GUEST_TEST -DSMP_CONTROLLER_TEST -DSMP_KICK_TEST -DSMP_MAINTENANCE_TEST -c $< -o $@
build/krnl-smp-maintenance.elf: $(KCOMMON) build/k-smp-main.o build/k-smp-maintenance-probe.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-shootdown.o build/k-cache-rendezvous.o build/k-cache-maintenance.o build/k-cache-page.o build/k-smp-guest.o build/k-smp-ept.o build/k-smp-guest-entry.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-maintenance-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-maintenance
smp-maintenance: build/smp-maintenance-esp/EFI/BOOT/BOOTX64.EFI build/smp-maintenance-esp/syle/krnl.elf


build/k-cache-mp-transition.o: ../krnl/cache_mp_transition.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-cache-ept-alias.o: ../krnl/cache_ept_alias.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-smp-cache-transition-probe.o: ../krnl/smp.c ../krnl/smp_cache_transition.inc | build
	$(CC) $(KFLAGS) -DSMP_IPI_TEST -DSMP_VMX_TEST -DSMP_GUEST_TEST -DSMP_CONTROLLER_TEST -DSMP_KICK_TEST -DSMP_MAINTENANCE_TEST -DSMP_MP_TYPE_TEST -c $< -o $@
build/krnl-smp-cache-transition.elf: $(KCOMMON) build/k-smp-main.o build/k-smp-cache-transition-probe.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-shootdown.o build/k-cache-rendezvous.o build/k-cache-maintenance.o build/k-cache-page.o build/k-cache-mp-transition.o build/k-cache-ept-alias.o build/k-smp-guest.o build/k-smp-ept.o build/k-smp-guest-entry.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-cache-transition-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-cache-transition
smp-cache-transition: build/smp-cache-transition-esp/EFI/BOOT/BOOTX64.EFI build/smp-cache-transition-esp/syle/krnl.elf

# N32A selected nested paths use an explicit per-call vCPU frame/fixture.
build/k-nested32-probe.o: ../krnl/vmx.c | build
	$(CC) $(KFLAGS) -DVMX_ROOT_TEST -DVMX_GUEST_TEST -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_VCPU_TEST -c $< -o $@
build/k-nested-vcpu-policy.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) -DVMX_NESTED_TEST -c $< -o $@
build/k-nested-vcpu-entry.o: ../krnl/nested_vcpu_entry.S | build
	$(CC) -g -fno-pie -c $< -o $@
build/k-nested-host-state.o: ../krnl/nested_host_state.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested32-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_VCPU_TEST -c $< -o $@
build/k-nested32-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_VCPU_TEST -c $< -o $@
build/krnl-vmx-nested-context.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-nested32-probe.o build/k-nested32-guest.o build/k-nested32-adapter.o build/k-nested-ept.o build/k-nested-payload.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested-vcpu-policy.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-context-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-context
vmx-nested-context: build/vmx-nested-context-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-context-esp/syle/krnl.elf
build/k-nested32pf-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_VCPU_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -DVMX_NESTED_PF_INJECT_TEST -c $< -o $@
build/k-nested32pf-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_VCPU_TEST -DVMX_NESTED_EVENT_TEST -DVMX_NESTED_INJECT_TEST -DVMX_NESTED_GP_INJECT_TEST -DVMX_NESTED_PF_INJECT_TEST -c $< -o $@
build/krnl-vmx-nested-context-pf.elf: $(filter-out build/k-vmx.o,$(KCOMMON)) build/k-nested32-probe.o build/k-nested32pf-guest.o build/k-nested32pf-adapter.o build/k-nested8-ept.o build/k-nested8-payload.o build/k-nested_events.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested-vcpu-policy.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-smoke.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/vmx-nested-context-pf-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smoke.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: vmx-nested-context-pf
vmx-nested-context-pf: build/vmx-nested-context-pf-esp/EFI/BOOT/BOOTX64.EFI build/vmx-nested-context-pf-esp/syle/krnl.elf

# N32B two independent nested instances; all resources prepared by BSP.
N32BFLAGS = -DVMX_EPT_TEST -DVMX_NESTED_TEST -DVMX_NESTED_VCPU_TEST -DVMX_NESTED_PARALLEL_TEST
build/k-nested32b-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N32BFLAGS) -c $< -o $@
build/k-nested32b-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N32BFLAGS) -c $< -o $@
build/k-nested32b-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N32BFLAGS) -c $< -o $@
build/k-nested32b-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc | build
	$(CC) -g -fno-pie $(N32BFLAGS) -c $< -o $@
build/k-nested32b-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc | build
	$(CC) $(KFLAGS) $(N32BFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested.elf: $(KCOMMON) build/k-smp-main.o build/k-nested32b-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested32b-runtime.o build/k-nested32b-guest.o build/k-nested32b-adapter.o build/k-nested32b-payload.o build/k-nested-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested-vcpu-policy.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested
smp-nested: build/smp-nested-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-esp/syle/krnl.elf

# N33B one immutable shared L1 GPA domain with dispatch-level ownership.
N33BFLAGS = $(N32BFLAGS) -DVMX_NESTED_DOMAIN_TEST
build/k-domain-policy.o: ../krnl/nested_domain.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-domain-dispatch.o: ../krnl/nested_domain_dispatch.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-domain-memory.o: ../krnl/nested_domain_memory.c | build
	$(CC) $(KFLAGS) $(N33BFLAGS) -c $< -o $@
build/k-nested33b-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N33BFLAGS) -c $< -o $@
build/k-nested33b-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N33BFLAGS) -c $< -o $@
build/k-nested33b-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N33BFLAGS) -c $< -o $@
build/k-nested33b-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N33BFLAGS) -c $< -o $@
build/k-nested33b-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N33BFLAGS) -c $< -o $@
build/k-nested33b-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc | build
	$(CC) -g -fno-pie $(N33BFLAGS) -c $< -o $@
build/k-nested33b-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc | build
	$(CC) $(KFLAGS) $(N33BFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-domain.elf: $(KCOMMON) build/k-smp-main.o build/k-nested33b-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested33b-runtime.o build/k-nested33b-guest.o build/k-nested33b-adapter.o build/k-nested33b-payload.o build/k-nested33b-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested33b-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-domain-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-domain
smp-nested-domain: build/smp-nested-domain-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-domain-esp/syle/krnl.elf

# N33C stopped-owner domain backing revocation.
N33CFLAGS = $(N33BFLAGS) -DVMX_NESTED_RECLAIM_TEST
build/k-nested33c-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N33CFLAGS) -c $< -o $@
build/k-nested33c-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N33CFLAGS) -c $< -o $@
build/k-nested33c-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N33CFLAGS) -c $< -o $@
build/k-nested33c-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N33CFLAGS) -c $< -o $@
build/k-nested33c-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N33CFLAGS) -c $< -o $@
build/k-nested33c-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc | build
	$(CC) -g -fno-pie $(N33CFLAGS) -c $< -o $@
build/k-nested33c-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_reclaim_runtime.inc | build
	$(CC) $(KFLAGS) $(N33CFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-reclaim.elf: $(KCOMMON) build/k-smp-main.o build/k-nested33c-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested33c-runtime.o build/k-nested33c-guest.o build/k-nested33c-adapter.o build/k-nested33c-payload.o build/k-nested33c-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested33c-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-reclaim-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-reclaim
smp-nested-reclaim: build/smp-nested-reclaim-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-reclaim-esp/syle/krnl.elf

N33DFLAGS = $(N33CFLAGS) -DVMX_NESTED_DOMAIN_KICK_TEST
build/k-nested-kick.o: ../krnl/nested_kick.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested33d-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N33DFLAGS) -c $< -o $@
build/k-nested33d-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N33DFLAGS) -c $< -o $@
build/k-nested33d-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N33DFLAGS) -c $< -o $@
build/k-nested33d-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N33DFLAGS) -c $< -o $@
build/k-nested33d-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N33DFLAGS) -c $< -o $@
build/k-nested33d-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc | build
	$(CC) -g -fno-pie $(N33DFLAGS) -c $< -o $@
build/k-nested33d-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_reclaim_runtime.inc | build
	$(CC) $(KFLAGS) $(N33DFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-kick.elf: $(KCOMMON) build/k-smp-main.o build/k-nested33d-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested33d-runtime.o build/k-nested33d-guest.o build/k-nested33d-adapter.o build/k-nested33d-payload.o build/k-nested33d-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested33d-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-nested-kick.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-kick-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-kick
smp-nested-kick: build/smp-nested-kick-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-kick-esp/syle/krnl.elf

N34AFLAGS = $(N33BFLAGS) -DVMX_NESTED_OWNER_SPARSE_TEST
build/k-owner-pool.o: ../krnl/nested_owner_pool.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-owner-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N34AFLAGS) -c $< -o $@
build/k-nested34a-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N34AFLAGS) -c $< -o $@
build/k-nested34a-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N34AFLAGS) -c $< -o $@
build/k-nested34a-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N34AFLAGS) -c $< -o $@
build/k-nested34a-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N34AFLAGS) -c $< -o $@
build/k-nested34a-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N34AFLAGS) -c $< -o $@
build/k-nested34a-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc | build
	$(CC) -g -fno-pie $(N34AFLAGS) -c $< -o $@
build/k-nested34a-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc | build
	$(CC) $(KFLAGS) $(N34AFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-sparse.elf: $(KCOMMON) build/k-smp-main.o build/k-nested34a-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested34a-runtime.o build/k-nested34a-guest.o build/k-nested34a-adapter.o build/k-nested34a-payload.o build/k-nested34a-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested34a-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-owner-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-sparse-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-sparse
smp-nested-sparse: build/smp-nested-sparse-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-sparse-esp/syle/krnl.elf


N34BFLAGS = $(N34AFLAGS) -DVMX_NESTED_LOCAL_INVEPT_TEST
build/k-local-invept.o: ../krnl/nested_local_invept.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested34b-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N34BFLAGS) -c $< -o $@
build/k-nested34b-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N34BFLAGS) -c $< -o $@
build/k-nested34b-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N34BFLAGS) -c $< -o $@
build/k-nested34b-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N34BFLAGS) -c $< -o $@
build/k-nested34b-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N34BFLAGS) -c $< -o $@
build/k-nested34b-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc | build
	$(CC) -g -fno-pie $(N34BFLAGS) -c $< -o $@
build/k-nested34b-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_local_runtime.inc | build
	$(CC) $(KFLAGS) $(N34BFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-local-invept.elf: $(KCOMMON) build/k-smp-main.o build/k-nested34b-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested34b-runtime.o build/k-nested34b-guest.o build/k-nested34b-adapter.o build/k-nested34b-payload.o build/k-nested34b-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested34b-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-local-invept.o build/k-owner-pool.o build/k-owner-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-local-invept-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-local-invept
smp-nested-local-invept: build/smp-nested-local-invept-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-local-invept-esp/syle/krnl.elf


N34CFLAGS = $(N34AFLAGS) -DVMX_NESTED_OWNER_CONTEXT_TEST
build/k-owner-context-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N34CFLAGS) -c $< -o $@
build/k-nested34c-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N34CFLAGS) -c $< -o $@
build/k-nested34c-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N34CFLAGS) -c $< -o $@
build/k-nested34c-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N34CFLAGS) -c $< -o $@
build/k-nested34c-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N34CFLAGS) -c $< -o $@
build/k-nested34c-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N34CFLAGS) -c $< -o $@
build/k-nested34c-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc ../krnl/nested_owner_context_l1.inc | build
	$(CC) -g -fno-pie $(N34CFLAGS) -c $< -o $@
build/k-nested34c-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_context_runtime.inc | build
	$(CC) $(KFLAGS) $(N34CFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-owner-contexts.elf: $(KCOMMON) build/k-smp-main.o build/k-nested34c-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested34c-runtime.o build/k-nested34c-guest.o build/k-nested34c-adapter.o build/k-nested34c-payload.o build/k-nested34c-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested34c-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-local-invept.o build/k-owner-pool.o build/k-owner-context-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-owner-contexts-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-owner-contexts
smp-nested-owner-contexts: build/smp-nested-owner-contexts-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-owner-contexts-esp/syle/krnl.elf



N34C2FLAGS = $(N34CFLAGS) -DVMX_NESTED_OWNER_EVICT_TEST
build/k-nested34c2-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N34C2FLAGS) -c $< -o $@
build/k-nested34c2-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N34C2FLAGS) -c $< -o $@
build/k-nested34c2-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N34C2FLAGS) -c $< -o $@
build/k-nested34c2-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N34C2FLAGS) -c $< -o $@
build/k-nested34c2-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N34C2FLAGS) -c $< -o $@
build/k-nested34c2-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc ../krnl/nested_owner_context_l1.inc | build
	$(CC) -g -fno-pie $(N34C2FLAGS) -c $< -o $@
build/k-nested34c2-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_context_runtime.inc | build
	$(CC) $(KFLAGS) $(N34C2FLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-owner-evict.elf: $(KCOMMON) build/k-smp-main.o build/k-nested34c2-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested34c2-runtime.o build/k-nested34c2-guest.o build/k-nested34c2-adapter.o build/k-nested34c2-payload.o build/k-nested34c2-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested34c2-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-local-invept.o build/k-owner-pool.o build/k-owner-context-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-owner-evict-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-owner-evict
smp-nested-owner-evict: build/smp-nested-owner-evict-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-owner-evict-esp/syle/krnl.elf




# N35B1 single immutable shared AD1 root; old serialized paths stay opt-in.
N35BFLAGS = $(N34AFLAGS) -DVMX_NESTED_OWNER_AD_TEST
build/k-owner-ad-sparse.o: ../krnl/nested_owner_sparse.c ../krnl/nested_owner_ad.h | build
	$(CC) $(KFLAGS) $(N35BFLAGS) -c $< -o $@
build/k-owner-ad.o: ../krnl/nested_owner_ad.c | build
	$(CC) $(KFLAGS) $(N35BFLAGS) -c $< -o $@
build/k-atomic-ept.o: ../krnl/nested_ept_atomic.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-ad-updater-gate.o: ../krnl/ad_updater_gate.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested35b-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N35BFLAGS) -c $< -o $@
build/k-nested35b-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N35BFLAGS) -c $< -o $@
build/k-nested35b-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N35BFLAGS) -c $< -o $@
build/k-nested35b-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N35BFLAGS) -c $< -o $@
build/k-nested35b-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N35BFLAGS) -c $< -o $@
build/k-nested35b-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc | build
	$(CC) -g -fno-pie $(N35BFLAGS) -c $< -o $@
build/k-nested35b-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc | build
	$(CC) $(KFLAGS) $(N35BFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-atomic-ad.elf: $(KCOMMON) build/k-smp-main.o build/k-nested35b-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested35b-runtime.o build/k-nested35b-guest.o build/k-nested35b-adapter.o build/k-nested35b-payload.o build/k-nested35b-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested35b-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-owner-ad-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o build/k-owner-ad.o build/k-atomic-ept.o build/k-ad-updater-gate.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-atomic-ad-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-atomic-ad
smp-nested-atomic-ad: build/smp-nested-atomic-ad-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-atomic-ad-esp/syle/krnl.elf



N35CFLAGS = $(N35BFLAGS) -DVMX_NESTED_OWNER_AD_RESET_TEST
build/k-nested35c-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N35CFLAGS) -c $< -o $@
build/k-nested35c-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N35CFLAGS) -c $< -o $@
build/k-nested35c-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N35CFLAGS) -c $< -o $@
build/k-nested35c-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N35CFLAGS) -c $< -o $@
build/k-nested35c-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N35CFLAGS) -c $< -o $@
build/k-nested35c-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc | build
	$(CC) -g -fno-pie $(N35CFLAGS) -c $< -o $@
build/k-nested35c-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc | build
	$(CC) $(KFLAGS) $(N35CFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-ad-reset.elf: $(KCOMMON) build/k-smp-main.o build/k-nested35c-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested35c-runtime.o build/k-nested35c-guest.o build/k-nested35c-adapter.o build/k-nested35c-payload.o build/k-nested35c-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested35c-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-owner-ad-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o build/k-owner-ad.o build/k-atomic-ept.o build/k-ad-updater-gate.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-ad-reset-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-ad-reset
smp-nested-ad-reset: build/smp-nested-ad-reset-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-ad-reset-esp/syle/krnl.elf


# N36B shared-domain stopped-owner typed UC transitions.
N36BFLAGS = $(N34AFLAGS) -DVMX_NESTED_OWNER_TYPED_TEST
build/k-nested36b-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N36BFLAGS) -c $< -o $@
build/k-nested36b-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N36BFLAGS) -c $< -o $@
build/k-nested36b-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N36BFLAGS) -c $< -o $@
build/k-nested36b-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N36BFLAGS) -c $< -o $@
build/k-nested36b-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N36BFLAGS) -c $< -o $@
build/k-nested36b-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc | build
	$(CC) -g -fno-pie $(N36BFLAGS) -c $< -o $@
build/k-nested36b-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc | build
	$(CC) $(KFLAGS) $(N36BFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-typed-uc.elf: $(KCOMMON) build/k-smp-main.o build/k-nested36b-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested36b-runtime.o build/k-nested36b-guest.o build/k-nested36b-adapter.o build/k-nested36b-payload.o build/k-nested36b-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested36b-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-owner-typed-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o build/k-owner-typed.o build/k-domain-typed-lease.o build/k-domain-typed-inventory.o build/k-domain-typed-alias-plan.o build/k-cache-maintenance.o build/k-cache-page.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-typed-uc-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-typed-uc
smp-nested-typed-uc: build/smp-nested-typed-uc-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-typed-uc-esp/syle/krnl.elf

build/k-owner-typed-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N36BFLAGS) -c $< -o $@
build/k-owner-typed.o: ../krnl/nested_owner_typed.c | build
	$(CC) $(KFLAGS) $(N36BFLAGS) -c $< -o $@
build/k-domain-typed-lease.o: ../krnl/domain_typed_lease.c | build
	$(CC) $(KFLAGS) $(N36BFLAGS) -c $< -o $@
build/k-domain-typed-inventory.o: ../krnl/domain_typed_inventory.c | build
	$(CC) $(KFLAGS) $(N36BFLAGS) -c $< -o $@
build/k-domain-typed-alias-plan.o: ../krnl/domain_typed_alias_plan.c | build
	$(CC) $(KFLAGS) $(N36BFLAGS) -c $< -o $@

# N37 uses the same maintenance protocol with explicit existing-PAT WT policy.
N37FLAGS = $(N36BFLAGS) -DVMX_NESTED_OWNER_TYPED_WT_TEST
build/k-nested37-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N37FLAGS) -c $< -o $@
build/k-nested37-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N37FLAGS) -c $< -o $@
build/k-nested37-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N37FLAGS) -c $< -o $@
build/k-nested37-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N37FLAGS) -c $< -o $@
build/k-nested37-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N37FLAGS) -c $< -o $@
build/k-nested37-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc | build
	$(CC) -g -fno-pie $(N37FLAGS) -c $< -o $@
build/k-nested37-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc | build
	$(CC) $(KFLAGS) $(N37FLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-typed-wt.elf: $(KCOMMON) build/k-smp-main.o build/k-nested37-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested37-runtime.o build/k-nested37-guest.o build/k-nested37-adapter.o build/k-nested37-payload.o build/k-nested37-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested37-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-owner-typed-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o build/k-owner-typed.o build/k-domain-typed-lease.o build/k-domain-typed-inventory.o build/k-domain-typed-alias-plan.o build/k-cache-maintenance.o build/k-cache-page.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-typed-wt-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-typed-wt
smp-nested-typed-wt: build/smp-nested-typed-wt-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-typed-wt-esp/syle/krnl.elf

# N38B fixed-membership per-owner typed contexts and directory transactions.
N38BFLAGS = $(N36BFLAGS) -DVMX_NESTED_OWNER_CONTEXT_TEST -DVMX_NESTED_OWNER_TYPED_CONTEXT_TEST
build/k-nested38b-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N38BFLAGS) -c $< -o $@
build/k-nested38b-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N38BFLAGS) -c $< -o $@
build/k-nested38b-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N38BFLAGS) -c $< -o $@
build/k-nested38b-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N38BFLAGS) -c $< -o $@
build/k-nested38b-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N38BFLAGS) -c $< -o $@
build/k-nested38b-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc | build
	$(CC) -g -fno-pie $(N38BFLAGS) -c $< -o $@
build/k-nested38b-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc | build
	$(CC) $(KFLAGS) $(N38BFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-typed-contexts.elf: $(KCOMMON) build/k-smp-main.o build/k-nested38b-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested38b-runtime.o build/k-nested38b-guest.o build/k-nested38b-adapter.o build/k-nested38b-payload.o build/k-nested38b-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested38b-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-owner-typed-context-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o build/k-owner-typed-context.o build/k-domain-typed-lease.o build/k-domain-typed-inventory.o build/k-domain-typed-alias-plan.o build/k-cache-maintenance.o build/k-cache-page.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-typed-contexts-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-typed-contexts
smp-nested-typed-contexts: build/smp-nested-typed-contexts-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-typed-contexts-esp/syle/krnl.elf

build/k-owner-typed-context-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N38BFLAGS) -c $< -o $@
build/k-owner-typed-context.o: ../krnl/nested_owner_typed.c | build
	$(CC) $(KFLAGS) $(N38BFLAGS) -c $< -o $@
build/k-typed-directory.o: ../krnl/typed_directory.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-typed-directory-registry.o: ../krnl/typed_directory_registry.c | build
	$(CC) $(KFLAGS) -c $< -o $@

# N38C-B runtime dynamic typed roots: UC, three virtual roots, budget two.
N38CBFLAGS = $(N38BFLAGS) -DVMX_NESTED_OWNER_TYPED_DYNAMIC_TEST -DVMX_NESTED_OWNER_EVICT_TEST
build/k-nested38cb-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N38CBFLAGS) -c $< -o $@
build/k-nested38cb-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N38CBFLAGS) -c $< -o $@
build/k-nested38cb-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N38CBFLAGS) -c $< -o $@
build/k-nested38cb-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N38CBFLAGS) -c $< -o $@
build/k-nested38cb-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N38CBFLAGS) -c $< -o $@
build/k-nested38cb-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc ../krnl/nested_typed_dynamic_l1.inc ../krnl/nested_typed_dynamic_l2.inc | build
	$(CC) -g -fno-pie $(N38CBFLAGS) -c $< -o $@
build/k-nested38cb-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_typed_dynamic_maintenance.inc | build
	$(CC) $(KFLAGS) $(N38CBFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-typed-dynamic.elf: $(KCOMMON) build/k-smp-main.o build/k-nested38cb-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested38cb-runtime.o build/k-nested38cb-guest.o build/k-nested38cb-adapter.o build/k-nested38cb-payload.o build/k-nested38cb-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested38cb-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-owner-typed-dynamic-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o build/k-owner-dynamic-typed.o build/k-domain-typed-inventory.o build/k-domain-typed-alias-plan.o build/k-cache-maintenance.o build/k-cache-page.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o build/k-typed-root-lease.o build/k-typed-root-registry.o build/k-cache-mp-transition.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-typed-dynamic-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-typed-dynamic
smp-nested-typed-dynamic: build/smp-nested-typed-dynamic-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-typed-dynamic-esp/syle/krnl.elf

build/k-owner-typed-dynamic-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N38CBFLAGS) -c $< -o $@
build/k-owner-dynamic-typed.o: ../krnl/nested_owner_dynamic_typed.c | build
	$(CC) $(KFLAGS) $(N38CBFLAGS) -c $< -o $@
build/k-typed-root-lease.o: ../krnl/typed_root_lease.c | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-typed-root-registry.o: ../krnl/typed_root_registry.c | build
	$(CC) $(KFLAGS) -c $< -o $@

# N39B1 actual owner-local root-scoped provider; serial owner rounds retained.
N39B1FLAGS = $(N38CBFLAGS) -DVMX_NESTED_OWNER_ROOT_SCOPED_TEST
build/k-nested39b1-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N39B1FLAGS) -c $< -o $@
build/k-nested39b1-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N39B1FLAGS) -c $< -o $@
build/k-nested39b1-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N39B1FLAGS) -c $< -o $@
build/k-nested39b1-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N39B1FLAGS) -c $< -o $@
build/k-nested39b1-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N39B1FLAGS) -c $< -o $@
build/k-nested39b1-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc ../krnl/nested_typed_dynamic_l1.inc ../krnl/nested_typed_dynamic_l2.inc | build
	$(CC) -g -fno-pie $(N39B1FLAGS) -c $< -o $@
build/k-nested39b1-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_typed_dynamic_maintenance.inc | build
	$(CC) $(KFLAGS) $(N39B1FLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-typed-root-scoped.elf: $(KCOMMON) build/k-smp-main.o build/k-nested39b1-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested39b1-runtime.o build/k-nested39b1-guest.o build/k-nested39b1-adapter.o build/k-nested39b1-payload.o build/k-nested39b1-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested39b1-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-owner-root-scoped-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o build/k-owner-root-scoped-typed.o build/k-domain-typed-inventory.o build/k-domain-typed-alias-plan.o build/k-cache-maintenance.o build/k-cache-page.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o build/k-typed-root-lease.o build/k-typed-root-registry.o build/k-cache-mp-transition.o build/k-typed-root-local-provider.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-typed-root-scoped-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-typed-root-scoped
smp-nested-typed-root-scoped: build/smp-nested-typed-root-scoped-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-typed-root-scoped-esp/syle/krnl.elf

build/k-owner-root-scoped-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N39B1FLAGS) -c $< -o $@
build/k-owner-root-scoped-typed.o: ../krnl/nested_owner_dynamic_typed.c | build
	$(CC) $(KFLAGS) $(N39B1FLAGS) -c $< -o $@
build/k-typed-root-local-provider.o: ../krnl/typed_root_local_provider.c ../krnl/typed_root_local_provider.h | build
	$(CC) $(KFLAGS) -c $< -o $@

# N39B2 actual overlapping owner-private churn with immutable causal evidence.
N39B2FLAGS = $(N39B1FLAGS) -DVMX_NESTED_OWNER_OVERLAP_TEST
build/k-nested39b2-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N39B2FLAGS) -c $< -o $@
build/k-nested39b2-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N39B2FLAGS) -c $< -o $@
build/k-nested39b2-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N39B2FLAGS) -c $< -o $@
build/k-nested39b2-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N39B2FLAGS) -c $< -o $@
build/k-nested39b2-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N39B2FLAGS) -c $< -o $@
build/k-nested39b2-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc ../krnl/nested_typed_dynamic_l1.inc ../krnl/nested_typed_dynamic_l2.inc | build
	$(CC) -g -fno-pie $(N39B2FLAGS) -c $< -o $@
build/k-nested39b2-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_typed_dynamic_maintenance.inc | build
	$(CC) $(KFLAGS) $(N39B2FLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-typed-overlap.elf: $(KCOMMON) build/k-smp-main.o build/k-nested39b2-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested39b2-runtime.o build/k-nested39b2-guest.o build/k-nested39b2-adapter.o build/k-nested39b2-payload.o build/k-nested39b2-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested39b2-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-owner-overlap-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o build/k-owner-overlap-typed.o build/k-domain-typed-inventory.o build/k-domain-typed-alias-plan.o build/k-cache-maintenance.o build/k-cache-page.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o build/k-typed-root-lease.o build/k-typed-root-registry.o build/k-cache-mp-transition.o build/k-typed-root-local-provider.o build/k-typed-root-overlap.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-typed-overlap-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-typed-overlap
smp-nested-typed-overlap: build/smp-nested-typed-overlap-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-typed-overlap-esp/syle/krnl.elf

build/k-owner-overlap-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N39B2FLAGS) -c $< -o $@
build/k-owner-overlap-typed.o: ../krnl/nested_owner_dynamic_typed.c | build
	$(CC) $(KFLAGS) $(N39B2FLAGS) -c $< -o $@

build/k-typed-root-overlap.o: ../krnl/typed_root_overlap.c ../krnl/typed_root_overlap.h | build
	$(CC) $(KFLAGS) -c $< -o $@

# N40B immutable root catalog and authoritative owned backing, on the B2 protocol.
N40BFLAGS = $(N39B2FLAGS) -DVMX_NESTED_ROOT_CATALOG_TEST
build/k-nested40b-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N40BFLAGS) -c $< -o $@
build/k-nested40b-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N40BFLAGS) -c $< -o $@
build/k-nested40b-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N40BFLAGS) -c $< -o $@
build/k-nested40b-adapter.o: ../krnl/nested_guest.c ../krnl/nested_owner_dynamic_typed.h | build
	$(CC) $(KFLAGS) $(N40BFLAGS) -c $< -o $@
build/k-nested40b-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N40BFLAGS) -c $< -o $@
build/k-nested40b-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_parallel_l2.inc ../krnl/nested_typed_dynamic_l1.inc ../krnl/nested_typed_dynamic_l2.inc | build
	$(CC) -g -fno-pie $(N40BFLAGS) -c $< -o $@
build/k-nested40b-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_typed_dynamic_maintenance.inc | build
	$(CC) $(KFLAGS) $(N40BFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-root-catalog.elf: $(KCOMMON) build/k-smp-main.o build/k-nested40b-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested40b-runtime.o build/k-nested40b-guest.o build/k-nested40b-adapter.o build/k-nested40b-payload.o build/k-nested40b-ept.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested40b-vcpu.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-owner-catalog-sparse.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-domain-memory.o build/k-owner-catalog-typed.o build/k-domain-typed-inventory.o build/k-domain-typed-alias-plan.o build/k-cache-maintenance.o build/k-cache-page.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o build/k-typed-root-lease.o build/k-typed-root-registry.o build/k-cache-mp-transition.o build/k-typed-root-local-provider.o build/k-typed-root-overlap.o build/k-nested-root-catalog.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-root-catalog-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-root-catalog
smp-nested-root-catalog: build/smp-nested-root-catalog-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-root-catalog-esp/syle/krnl.elf

build/k-owner-catalog-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N40BFLAGS) -c $< -o $@
build/k-owner-catalog-typed.o: ../krnl/nested_owner_dynamic_typed.c ../krnl/nested_owner_catalog.inc ../krnl/nested_owner_dynamic_typed.h ../krnl/nested_root_catalog.h | build
	$(CC) $(KFLAGS) $(N40BFLAGS) -c $< -o $@

build/k-nested-root-catalog.o: ../krnl/nested_root_catalog.c ../krnl/nested_root_catalog.h | build
	$(CC) $(KFLAGS) -c $< -o $@

# N41C independent immutable multi-root WB/IPAT1/AD1 runtime.
N41CFLAGS = $(N34CFLAGS) -DVMX_NESTED_ATOMIC_CATALOG_TEST
build/k-nested41c-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N41CFLAGS) -c $< -o $@
build/k-nested41c-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N41CFLAGS) -c $< -o $@
build/k-nested41c-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N41CFLAGS) -c $< -o $@
build/k-nested41c-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N41CFLAGS) -c $< -o $@
build/k-nested41c-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N41CFLAGS) -c $< -o $@
build/k-nested41c-owner.o: ../krnl/nested_owner_atomic.c | build
	$(CC) $(KFLAGS) $(N41CFLAGS) -c $< -o $@
build/k-nested41c-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N41CFLAGS) -c $< -o $@
build/k-nested41c-memory.o: ../krnl/nested_atomic_memory.c | build
	$(CC) $(KFLAGS) $(N41CFLAGS) -c $< -o $@
build/k-nested41c-domain-memory.o: ../krnl/nested_domain_memory.c | build
	$(CC) $(KFLAGS) $(N41CFLAGS) -c $< -o $@
build/k-nested41c-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_atomic_catalog_l1.inc ../krnl/nested_atomic_catalog_l2.inc | build
	$(CC) -g -fno-pie $(N41CFLAGS) -c $< -o $@
build/k-nested41c-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_atomic_runtime.inc | build
	$(CC) $(KFLAGS) $(N41CFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/k-typed-root-atomic.o: ../krnl/typed_root_atomic.c ../krnl/typed_root_atomic.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/k-nested-atomic-catalog.o: ../krnl/nested_atomic_catalog.c ../krnl/nested_atomic_catalog.h | build
	$(CC) $(KFLAGS) -c $< -o $@
build/krnl-smp-nested-atomic-catalog.elf: $(KCOMMON) build/k-smp-main.o build/k-nested41c-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested41c-vcpu.o build/k-nested41c-runtime.o build/k-nested41c-guest.o build/k-nested41c-adapter.o build/k-nested41c-ept.o build/k-nested41c-owner.o build/k-nested41c-sparse.o build/k-nested41c-memory.o build/k-nested41c-domain-memory.o build/k-nested41c-payload.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o build/k-typed-root-lease.o build/k-typed-root-registry.o build/k-typed-root-local-provider.o build/k-nested-root-catalog.o build/k-typed-root-atomic.o build/k-nested-atomic-catalog.o build/k-atomic-ept.o build/k-ad-updater-gate.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-atomic-catalog-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-atomic-catalog
smp-nested-atomic-catalog: build/smp-nested-atomic-catalog-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-atomic-catalog-esp/syle/krnl.elf

# N42B opt-in actual clear/retrack runtime; all profile-sensitive objects isolated.
N42BFLAGS = $(N41CFLAGS) -DVMX_NESTED_ATOMIC_RETRACK_TEST
build/k-nested42b-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -c $< -o $@
build/k-nested42b-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -c $< -o $@
build/k-nested42b-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -c $< -o $@
build/k-nested42b-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -c $< -o $@
build/k-nested42b-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -c $< -o $@
build/k-nested42b-owner.o: ../krnl/nested_owner_atomic.c | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -c $< -o $@
build/k-nested42b-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -c $< -o $@
build/k-nested42b-memory.o: ../krnl/nested_atomic_memory.c | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -c $< -o $@
build/k-nested42b-domain-memory.o: ../krnl/nested_domain_memory.c | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -c $< -o $@
build/k-nested42b-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_atomic_retrack_l1.inc ../krnl/nested_atomic_retrack_l2.inc | build
	$(CC) -g -fno-pie $(N42BFLAGS) -c $< -o $@
build/k-nested42b-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_atomic_retrack_runtime.inc | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-atomic-retrack.elf: $(KCOMMON) build/k-smp-main.o build/k-nested42b-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested42b-vcpu.o build/k-nested42b-runtime.o build/k-nested42b-guest.o build/k-nested42b-adapter.o build/k-nested42b-ept.o build/k-nested42b-owner.o build/k-nested42b-sparse.o build/k-nested42b-memory.o build/k-nested42b-domain-memory.o build/k-nested42b-payload.o build/k-nested42b-maintenance.o build/k-nested42b-retrack.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o build/k-typed-root-lease.o build/k-typed-root-registry.o build/k-typed-root-local-provider.o build/k-nested-root-catalog.o build/k-typed-root-atomic.o build/k-nested-atomic-catalog.o build/k-atomic-ept.o build/k-ad-updater-gate.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-atomic-retrack-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-atomic-retrack
smp-nested-atomic-retrack: build/smp-nested-atomic-retrack-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-atomic-retrack-esp/syle/krnl.elf

build/k-nested42b-maintenance.o: ../krnl/nested_owner_atomic_maintenance.c ../krnl/nested_owner_atomic_maintenance.h | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -c $< -o $@
build/k-nested42b-retrack.o: ../krnl/nested_atomic_retrack.c ../krnl/nested_atomic_retrack.h | build
	$(CC) $(KFLAGS) $(N42BFLAGS) -c $< -o $@

# Keep cached profile objects ABI-coherent when shared owner/vCPU layouts change.
NESTED_ATOMIC_LAYOUT_HEADERS = ../krnl/nested_parallel.h ../krnl/nested_owner_atomic.h ../krnl/nested_owner_sparse.h ../krnl/nested_vcpu.h
N41C_LAYOUT_OBJECTS = $(addprefix build/k-nested41c-,vcpu.o runtime.o guest.o adapter.o ept.o owner.o sparse.o memory.o domain-memory.o payload.o smp.o)
N42B_LAYOUT_OBJECTS = $(addprefix build/k-nested42b-,vcpu.o runtime.o guest.o adapter.o ept.o owner.o sparse.o memory.o domain-memory.o payload.o smp.o maintenance.o retrack.o)
$(N41C_LAYOUT_OBJECTS): $(NESTED_ATOMIC_LAYOUT_HEADERS)
$(N42B_LAYOUT_OBJECTS): $(NESTED_ATOMIC_LAYOUT_HEADERS) ../krnl/nested_owner_atomic_maintenance.h ../krnl/nested_atomic_retrack.h
build/k-domain-dispatch.o: ../krnl/nested_domain_dispatch.h
build/k-typed-root-atomic.o: ../krnl/typed_root_atomic_maintenance.inc ../krnl/typed_root_atomic_maintenance.h

# N43 opt-in actual clear/retrack runtime; all profile-sensitive objects isolated.
N43FLAGS = $(N42BFLAGS) -DVMX_NESTED_ATOMIC_MULTIROUND_TEST
build/k-nested43-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@
build/k-nested43-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@
build/k-nested43-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@
build/k-nested43-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@
build/k-nested43-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@
build/k-nested43-owner.o: ../krnl/nested_owner_atomic.c | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@
build/k-nested43-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@
build/k-nested43-memory.o: ../krnl/nested_atomic_memory.c | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@
build/k-nested43-domain-memory.o: ../krnl/nested_domain_memory.c | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@
build/k-nested43-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_atomic_multiround_l1.inc ../krnl/nested_atomic_multiround_l2.inc | build
	$(CC) -g -fno-pie $(N43FLAGS) -c $< -o $@
build/k-nested43-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_atomic_multiround_runtime.inc | build
	$(CC) $(KFLAGS) $(N43FLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-atomic-multiround.elf: $(KCOMMON) build/k-smp-main.o build/k-nested43-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested43-vcpu.o build/k-nested43-runtime.o build/k-nested43-guest.o build/k-nested43-adapter.o build/k-nested43-ept.o build/k-nested43-owner.o build/k-nested43-sparse.o build/k-nested43-memory.o build/k-nested43-domain-memory.o build/k-nested43-payload.o build/k-nested43-maintenance.o build/k-nested43-retrack.o build/k-nested43-multiround.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o build/k-typed-root-lease.o build/k-typed-root-registry.o build/k-typed-root-local-provider.o build/k-nested-root-catalog.o build/k-typed-root-atomic.o build/k-nested-atomic-catalog.o build/k-atomic-ept.o build/k-ad-updater-gate.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-atomic-multiround-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-atomic-multiround
smp-nested-atomic-multiround: build/smp-nested-atomic-multiround-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-atomic-multiround-esp/syle/krnl.elf

build/k-nested43-maintenance.o: ../krnl/nested_owner_atomic_maintenance.c ../krnl/nested_owner_atomic_maintenance.h | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@
build/k-nested43-retrack.o: ../krnl/nested_atomic_retrack.c ../krnl/nested_atomic_retrack.h | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@

build/k-nested43-multiround.o: ../krnl/nested_atomic_multiround.c ../krnl/nested_atomic_multiround.h | build
	$(CC) $(KFLAGS) $(N43FLAGS) -c $< -o $@
N43_LAYOUT_OBJECTS = $(addprefix build/k-nested43-,vcpu.o runtime.o guest.o adapter.o ept.o owner.o sparse.o memory.o domain-memory.o payload.o smp.o maintenance.o retrack.o multiround.o)
$(N43_LAYOUT_OBJECTS): $(NESTED_ATOMIC_LAYOUT_HEADERS) ../krnl/nested_owner_atomic_maintenance.h ../krnl/nested_atomic_retrack.h ../krnl/nested_atomic_multiround.h

# N44 opt-in actual clear/retrack runtime; all profile-sensitive objects isolated.
N44FLAGS = $(N43FLAGS) -DVMX_NESTED_ATOMIC_LARGEPAGE_TEST
build/k-nested44-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@
build/k-nested44-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@
build/k-nested44-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@
build/k-nested44-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@
build/k-nested44-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@
build/k-nested44-owner.o: ../krnl/nested_owner_atomic.c | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@
build/k-nested44-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@
build/k-nested44-memory.o: ../krnl/nested_atomic_memory.c | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@
build/k-nested44-domain-memory.o: ../krnl/nested_domain_memory.c | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@
build/k-nested44-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_atomic_multiround_l1.inc ../krnl/nested_atomic_largepage_l2.inc | build
	$(CC) -g -fno-pie $(N44FLAGS) -c $< -o $@
build/k-nested44-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_atomic_largepage_runtime.inc | build
	$(CC) $(KFLAGS) $(N44FLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-atomic-largepage.elf: $(KCOMMON) build/k-smp-main.o build/k-nested44-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested44-vcpu.o build/k-nested44-runtime.o build/k-nested44-guest.o build/k-nested44-adapter.o build/k-nested44-ept.o build/k-nested44-owner.o build/k-nested44-sparse.o build/k-nested44-memory.o build/k-nested44-domain-memory.o build/k-nested44-payload.o build/k-nested44-maintenance.o build/k-nested44-retrack.o build/k-nested44-multiround.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o build/k-typed-root-lease.o build/k-typed-root-registry.o build/k-typed-root-local-provider.o build/k-nested-root-catalog.o build/k-typed-root-atomic.o build/k-nested-atomic-catalog.o build/k-atomic-ept.o build/k-ad-updater-gate.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-atomic-largepage-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-atomic-largepage
smp-nested-atomic-largepage: build/smp-nested-atomic-largepage-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-atomic-largepage-esp/syle/krnl.elf

build/k-nested44-maintenance.o: ../krnl/nested_owner_atomic_maintenance.c ../krnl/nested_owner_atomic_maintenance.h | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@
build/k-nested44-retrack.o: ../krnl/nested_atomic_retrack.c ../krnl/nested_atomic_retrack.h | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@

build/k-nested44-multiround.o: ../krnl/nested_atomic_multiround.c ../krnl/nested_atomic_multiround.h | build
	$(CC) $(KFLAGS) $(N44FLAGS) -c $< -o $@
N44_LAYOUT_OBJECTS = $(addprefix build/k-nested44-,vcpu.o runtime.o guest.o adapter.o ept.o owner.o sparse.o memory.o domain-memory.o payload.o smp.o maintenance.o retrack.o multiround.o)
$(N44_LAYOUT_OBJECTS): $(NESTED_ATOMIC_LAYOUT_HEADERS) ../krnl/nested_owner_atomic_maintenance.h ../krnl/nested_atomic_retrack.h ../krnl/nested_atomic_multiround.h
# N44 format/trace ABI dependencies, including shared compiled core objects.
build/k-atomic-ept.o build/k-nested-atomic-catalog.o build/k-typed-root-atomic.o: ../krnl/nested_ept_atomic_format.h
$(N44_LAYOUT_OBJECTS): ../krnl/nested_owner_atomic.h ../krnl/nested_ept_atomic.h ../krnl/nested_root_catalog.h ../krnl/nested_atomic_memory.h ../krnl/typed_root_atomic.h

# N45 opt-in actual clear/retrack runtime; all profile-sensitive objects isolated.
N45FLAGS = $(N43FLAGS) -DVMX_NESTED_ATOMIC_GIGAPAGE_TEST
build/k-nested45-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@
build/k-nested45-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@
build/k-nested45-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@
build/k-nested45-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@
build/k-nested45-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@
build/k-nested45-owner.o: ../krnl/nested_owner_atomic.c | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@
build/k-nested45-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@
build/k-nested45-memory.o: ../krnl/nested_atomic_memory.c | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@
build/k-nested45-domain-memory.o: ../krnl/nested_domain_memory.c | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@
build/k-nested45-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_atomic_multiround_l1.inc ../krnl/nested_atomic_gigapage_l2.inc | build
	$(CC) -g -fno-pie $(N45FLAGS) -c $< -o $@
build/k-nested45-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_atomic_gigapage_runtime.inc | build
	$(CC) $(KFLAGS) $(N45FLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-atomic-gigapage.elf: $(KCOMMON) build/k-smp-main.o build/k-nested45-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested45-vcpu.o build/k-nested45-runtime.o build/k-nested45-guest.o build/k-nested45-adapter.o build/k-nested45-ept.o build/k-nested45-owner.o build/k-nested45-sparse.o build/k-nested45-memory.o build/k-nested45-domain-memory.o build/k-nested45-payload.o build/k-nested45-maintenance.o build/k-nested45-retrack.o build/k-nested45-multiround.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o build/k-typed-root-lease.o build/k-typed-root-registry.o build/k-typed-root-local-provider.o build/k-nested-root-catalog.o build/k-typed-root-atomic.o build/k-nested-atomic-catalog.o build/k-atomic-ept.o build/k-ad-updater-gate.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-atomic-gigapage-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-atomic-gigapage
smp-nested-atomic-gigapage: build/smp-nested-atomic-gigapage-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-atomic-gigapage-esp/syle/krnl.elf

build/k-nested45-maintenance.o: ../krnl/nested_owner_atomic_maintenance.c ../krnl/nested_owner_atomic_maintenance.h | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@
build/k-nested45-retrack.o: ../krnl/nested_atomic_retrack.c ../krnl/nested_atomic_retrack.h | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@

build/k-nested45-multiround.o: ../krnl/nested_atomic_multiround.c ../krnl/nested_atomic_multiround.h | build
	$(CC) $(KFLAGS) $(N45FLAGS) -c $< -o $@
N45_LAYOUT_OBJECTS = $(addprefix build/k-nested45-,vcpu.o runtime.o guest.o adapter.o ept.o owner.o sparse.o memory.o domain-memory.o payload.o smp.o maintenance.o retrack.o multiround.o)
$(N45_LAYOUT_OBJECTS): $(NESTED_ATOMIC_LAYOUT_HEADERS) ../krnl/nested_owner_atomic_maintenance.h ../krnl/nested_atomic_retrack.h ../krnl/nested_atomic_multiround.h
# N45 format/trace ABI dependencies, including shared compiled core objects.
build/k-atomic-ept.o build/k-nested-atomic-catalog.o build/k-typed-root-atomic.o: ../krnl/nested_ept_atomic_format.h
$(N45_LAYOUT_OBJECTS): ../krnl/nested_owner_atomic.h ../krnl/nested_ept_atomic.h ../krnl/nested_root_catalog.h ../krnl/nested_atomic_memory.h ../krnl/typed_root_atomic.h

# N46B opt-in UC/IPAT1/AD1 actual guest profile; keep retained WB objects isolated.
N46BFLAGS = $(N43FLAGS) -DVMX_NESTED_ATOMIC_UC_TEST
build/k-nested46b-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@
build/k-nested46b-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@
build/k-nested46b-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@
build/k-nested46b-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@
build/k-nested46b-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@
build/k-nested46b-owner.o: ../krnl/nested_owner_atomic.c | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@
build/k-nested46b-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@
build/k-nested46b-memory.o: ../krnl/nested_atomic_memory.c | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@
build/k-nested46b-domain-memory.o: ../krnl/nested_domain_memory.c | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@
build/k-nested46b-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_atomic_multiround_l1.inc ../krnl/nested_atomic_multiround_l2.inc | build
	$(CC) -g -fno-pie $(N46BFLAGS) -c $< -o $@
build/k-nested46b-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_atomic_uc_runtime.inc ../krnl/nested_atomic_uc_bootstrap.inc | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-atomic-uc.elf: $(KCOMMON) build/k-smp-main.o build/k-nested46b-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested46b-vcpu.o build/k-nested46b-runtime.o build/k-nested46b-guest.o build/k-nested46b-adapter.o build/k-nested46b-ept.o build/k-nested46b-owner.o build/k-nested46b-sparse.o build/k-nested46b-memory.o build/k-nested46b-domain-memory.o build/k-nested46b-payload.o build/k-nested46b-maintenance.o build/k-nested46b-retrack.o build/k-nested46b-multiround.o build/k-nested46b-provision.o build/k-cache-maintenance.o build/k-cache-page.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o build/k-typed-root-lease.o build/k-typed-root-registry.o build/k-typed-root-local-provider.o build/k-nested-root-catalog.o build/k-typed-root-atomic.o build/k-nested-atomic-catalog.o build/k-atomic-ept.o build/k-ad-updater-gate.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-atomic-uc-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-atomic-uc
smp-nested-atomic-uc: build/smp-nested-atomic-uc-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-atomic-uc-esp/syle/krnl.elf

build/k-nested46b-maintenance.o: ../krnl/nested_owner_atomic_maintenance.c ../krnl/nested_owner_atomic_maintenance.h | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@
build/k-nested46b-retrack.o: ../krnl/nested_atomic_retrack.c ../krnl/nested_atomic_retrack.h | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@

build/k-nested46b-multiround.o: ../krnl/nested_atomic_multiround.c ../krnl/nested_atomic_multiround.h | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@
build/k-nested46b-provision.o: ../krnl/nested_atomic_uc_provision.c ../krnl/nested_atomic_uc_provision.h | build
	$(CC) $(KFLAGS) $(N46BFLAGS) -c $< -o $@

N46B_LAYOUT_OBJECTS = $(addprefix build/k-nested46b-,vcpu.o runtime.o guest.o adapter.o ept.o owner.o sparse.o memory.o domain-memory.o payload.o smp.o maintenance.o retrack.o multiround.o provision.o)
$(N46B_LAYOUT_OBJECTS): $(NESTED_ATOMIC_LAYOUT_HEADERS) ../krnl/nested_owner_atomic_maintenance.h ../krnl/nested_atomic_retrack.h ../krnl/nested_atomic_multiround.h

# Keep the UC profile coherent with typed source, host-lease and provision layouts.
$(N46B_LAYOUT_OBJECTS): ../krnl/nested_ept_atomic.h ../krnl/nested_ept_atomic_format.h ../krnl/nested_root_catalog.h ../krnl/nested_atomic_memory.h ../krnl/typed_root_atomic.h ../krnl/nested_atomic_uc_provision.h ../krnl/cache_maintenance.h ../krnl/cache_page.h
build/k-cache-maintenance.o: ../krnl/cache_maintenance.h ../krnl/cache_page.h ../krnl/paging.h ../krnl/vmx_memory.h
build/k-cache-page.o: ../krnl/cache_page.h

# N47B one retained WB4K source replacement, independent eight-checkpoint profile.
N47BFLAGS = $(N41CFLAGS) -DVMX_NESTED_SOURCE_REPLACE_TEST
build/k-nested47b-vcpu.o: ../krnl/nested_vcpu_policy.c | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-runtime.o: ../krnl/nested_parallel.c | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-guest.o: ../krnl/vmx_guest.c | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-adapter.o: ../krnl/nested_guest.c | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-ept.o: ../krnl/ept.c | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-owner.o: ../krnl/nested_owner_atomic.c | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-sparse.o: ../krnl/nested_owner_sparse.c | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-memory.o: ../krnl/nested_atomic_memory.c | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-domain-memory.o: ../krnl/nested_domain_memory.c | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-payload.o: ../krnl/ept_payload.S ../krnl/nested_payload.inc ../krnl/nested_source_replace_l1.inc ../krnl/nested_source_replace_l2.inc | build
	$(CC) -g -fno-pie $(N47BFLAGS) -c $< -o $@
build/k-nested47b-smp.o: ../krnl/smp.c ../krnl/smp_nested.inc ../krnl/nested_source_replace_runtime.inc | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -DSMP_NESTED_TEST -DSMP_VMX_TEST -c $< -o $@
build/krnl-smp-nested-source-replace.elf: $(KCOMMON) build/k-smp-main.o build/k-nested47b-smp.o build/k-smp-vmx.o build/k-smp-acpi.o build/k-smp-cpu.o build/k-smp-entry.o build/k-nested47b-vcpu.o build/k-nested47b-runtime.o build/k-nested47b-guest.o build/k-nested47b-adapter.o build/k-nested47b-ept.o build/k-nested47b-owner.o build/k-nested47b-sparse.o build/k-nested47b-memory.o build/k-nested47b-domain-memory.o build/k-nested47b-payload.o build/k-nested_vmx.o build/k-nested_operand.o build/k-nested_profile.o build/k-nested_ept.o build/k-nested-vcpu-entry.o build/k-nested-host-state.o build/k-owner-pool.o build/k-nested_ept_contexts.o build/k-nested_ept_sparse.o build/k-domain-policy.o build/k-domain-dispatch.o build/k-typed-directory.o build/k-typed-directory-registry.o build/k-local-invept.o build/k-typed-root-lease.o build/k-typed-root-registry.o build/k-typed-root-local-provider.o build/k-nested-root-catalog.o build/k-typed-root-atomic.o build/k-nested-atomic-catalog.o build/k-atomic-ept.o build/k-ad-updater-gate.o build/k-nested47b-owner-replace.o build/k-nested47b-round.o build/k-nested47b-fixture.o build/k-nested47b-replace-memory.o build/k-cache-page.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
build/smp-nested-source-replace-esp/EFI/BOOT/BOOTX64.EFI: build/stage0-smp.so
	mkdir -p $(@D)
	$(OBJCOPY) $(SECTIONS) --target=efi-app-x86_64 $< $@
.PHONY: smp-nested-source-replace
smp-nested-source-replace: build/smp-nested-source-replace-esp/EFI/BOOT/BOOTX64.EFI build/smp-nested-source-replace-esp/syle/krnl.elf

build/k-nested47b-owner-replace.o: ../krnl/nested_owner_source_replace.c ../krnl/nested_owner_source_replace.h | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-round.o: ../krnl/nested_source_replace_round.c ../krnl/nested_source_replace_round.h | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-fixture.o: ../krnl/nested_source_replace_fixture.c ../krnl/nested_source_replace_fixture.h | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
build/k-nested47b-replace-memory.o: ../krnl/nested_source_replace_memory.c ../krnl/nested_source_replace_memory.h | build
	$(CC) $(KFLAGS) $(N47BFLAGS) -c $< -o $@
N47B_LAYOUT_OBJECTS = $(addprefix build/k-nested47b-,vcpu.o runtime.o guest.o adapter.o ept.o owner.o sparse.o memory.o domain-memory.o payload.o smp.o owner-replace.o round.o fixture.o replace-memory.o)
$(N47B_LAYOUT_OBJECTS): $(NESTED_ATOMIC_LAYOUT_HEADERS) ../krnl/nested_owner_source_replace.h ../krnl/nested_source_replace_round.h ../krnl/typed_root_source_replace.h ../krnl/nested_source_replace_fixture.h
build/k-typed-root-atomic.o: ../krnl/typed_root_source_replace.inc ../krnl/typed_root_source_successor.inc ../krnl/typed_root_source_replace.h

# Separate experimental guest image; normal all target remains unchanged.
include simple-guest.mk

# Explicit Linux recovery probes; never part of default build.
include linux-first-exit-recovery.mk
