#!/bin/sh
set -eu
out=$1
prefix=$2
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$out"
cc -I"$prefix/include/efi" -I"$prefix/include/efi/x86_64" -ffreestanding -fpic -fshort-wchar -mno-red-zone -fno-stack-protector -fno-stack-check -mgeneral-regs-only -Wall -Wextra -Werror ${UEFI_DISK_CFLAGS:-} -c "$here/boot.c" -o "$out/boot.o"
ld -nostdlib -znocombreloc -T "$prefix/lib/elf_x86_64_efi.lds" -shared -Bsymbolic "$prefix/lib/crt0-efi-x86_64.o" "$out/boot.o" -L"$prefix/lib" -lefi -lgnuefi -o "$out/boot.so"
objcopy -j .rodata -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc -O efi-app-x86_64 "$out/boot.so" "$out/BOOTX64.EFI"
