# Companion makefile: make -f Makefile -f tests/wx_alias.mk wx-alias
.PHONY: wx-alias
wx-alias: build/krnl-wx-1.elf build/krnl-wx-2.elf build/krnl-wx-3.elf
build/k-ring3-wx-%.o: ../krnl/ring3.c | build
	$(CC) $(KFLAGS) -DRING3_CASE=0 -DWX_ALIAS_TEST=$* -c $< -o $@
build/krnl-wx-%.elf: $(KCOMMON) build/k-ring3-main.o build/k-ring3-wx-%.o build/k-user-entry.o build/user-image.o ../krnl/link.ld
	$(LD) -nostdlib -static -T ../krnl/link.ld $(filter %.o,$^) -o $@
