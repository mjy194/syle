#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT HUP INT TERM
repo=$PWD;efi=/workspace/shared/syle-deps/root/usr/include/efi
for variant in baseline patched; do
 mkdir -p "$t/$variant/efi/src" "$t/$variant/krnl"
 ln -s "$repo/include" "$t/$variant/include"
 for f in krnl/*.h; do ln -s "$repo/$f" "$t/$variant/$f"; done
 for f in efi/src/*.h; do ln -s "$repo/$f" "$t/$variant/$f"; done
 patch='';test "$variant" = baseline || patch='--patched'
 python3 tests/virtio_ro/nmi_guard_candidate.py "$t/$variant" --host $patch
 flags="-std=c11 -O1 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -fsanitize=address,undefined -g -I$efi -I$efi/x86_64"
 cc $flags "$t/$variant/efi/src/exceptions.c" "$t/$variant/krnl/ring3.c" krnl/guest_control.c tests/virtio_ro/nmi_guard_path_test.c -Wl,--gc-sections -o "$t/$variant/test"
 expected=1;test "$variant" = baseline || expected=0
 ASAN_OPTIONS=detect_leaks=0 "$t/$variant/test" 2 3 "$expected" 0
 ASAN_OPTIONS=detect_leaks=0 "$t/$variant/test" 2 3 "$expected" 0 running
 ASAN_OPTIONS=detect_leaks=0 "$t/$variant/test" 2 3 "$expected" 1
 ASAN_OPTIONS=detect_leaks=0 "$t/$variant/test" 2 0 0 0
 ASAN_OPTIONS=detect_leaks=0 "$t/$variant/test" 14 3 1 0
 ASAN_OPTIONS=detect_leaks=0 "$t/$variant/test" 13 3 1 0
 ASAN_OPTIONS=detect_leaks=0 "$t/$variant/test" 14 0 0 0
 # Compile unmodified privilege instructions in isolated production candidate.
 python3 tests/virtio_ro/nmi_guard_candidate.py "$t/$variant" $patch
 cc -std=c11 -O1 -ffreestanding -fshort-wchar -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-pic -fno-pie -mcmodel=small -DSYLE_KERNEL -fno-lto -I"$efi" -I"$efi/x86_64" -c "$t/$variant/efi/src/exceptions.c" -o "$t/$variant/exceptions.o"
 cc -std=c11 -O1 -ffreestanding -fshort-wchar -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-pic -fno-pie -mcmodel=small -DSYLE_KERNEL -fno-lto -I"$efi" -I"$efi/x86_64" -c "$t/$variant/krnl/ring3.c" -o "$t/$variant/ring3.o"
done
echo 'isolated vector2 candidate: host path + kernel ABI compile PASS; default source unchanged, no NMI execution'
