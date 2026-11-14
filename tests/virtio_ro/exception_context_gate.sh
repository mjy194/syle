#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT HUP INT TERM
efi=/workspace/shared/syle-deps/root/usr/include/efi
mkdir -p "$t/efi/src";ln -s "$PWD/include" "$t/include"
python3 tests/virtio_ro/exception_context_gate_candidate.py "$t/efi/src/exceptions.c" --host-bootstrap
# Relative headers for generated source; no public source edits.
ln -s "$PWD/efi/src/exceptions.h" "$t/efi/src/exceptions.h";ln -s "$PWD/efi/src/console.h" "$t/efi/src/console.h"
cc -std=c11 -O1 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -fsanitize=address,undefined -g -I"$efi" -I"$efi/x86_64" -DECG_SOURCE=\""$t/efi/src/exceptions.c"\" -DLVB_HOST_TEST tests/virtio_ro/exception_context_gate_test.c krnl/linux_vcpu_host.c krnl/linux_vcpu_prepare.c krnl/linux_vcpu_backend.c -Wl,--gc-sections -o "$t/test"
ASAN_OPTIONS=detect_leaks=0 "$t/test"
python3 tests/virtio_ro/exception_context_gate_candidate.py "$t/efi/src/exceptions.c"
cc -std=c11 -O1 -Wall -Wextra -Werror -ffreestanding -fshort-wchar -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-stack-check -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-pic -fno-pie -mcmodel=small -DSYLE_KERNEL -fno-lto -I"$efi" -I"$efi/x86_64" -c "$t/efi/src/exceptions.c" -o "$t/native.o"
objdump -d --disassemble=exceptions_owned_context "$t/native.o" > "$t/guard.asm"
for instruction in sidt sgdt str rdmsr; do grep -Eq "[[:space:]]$instruction([[:space:]]|$)" "$t/guard.asm"; done
echo 'native gate candidate kernel ABI/instruction presence PASS; not executed, mapping remains blocked'
