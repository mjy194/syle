#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../.." && pwd)
: "${EFI_INC:=/workspace/shared/syle-deps/root/usr/include/efi}"
test -r "$EFI_INC/efi.h"
# The caller can retain the artifact; no original build directory is modified.
out=${1:?usage: final_link.sh NEW_OUTPUT_DIRECTORY}
if test -e "$out"; then echo 'choose a new output directory' >&2; exit 1; fi
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/efi/build"
ln -s "$repo/krnl" "$tmp/krnl"
ln -s "$repo/include" "$tmp/include"
ln -s "$repo/boot" "$tmp/boot"
ln -s "$repo/efi/Makefile" "$tmp/efi/Makefile"
ln -s "$repo/efi/src" "$tmp/efi/src"
for f in "$repo"/efi/*.mk; do ln -s "$f" "$tmp/efi/$(basename "$f")"; done
flags='-std=c11 -O1 -ffreestanding -fshort-wchar -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-stack-check -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-pic -fno-pie -mcmodel=small -DSYLE_KERNEL -fno-lto -Wall -Wextra -Werror'
# shellcheck disable=SC2086
cc $flags -c "$here/native_candidate.c" -o "$tmp/authority.raw.o"
nm -g --defined-only "$tmp/authority.raw.o" | awk '$3 ~ /^(pmm_raw_|pmm_owned_|vlm_)/ {print $3}' > "$tmp/private"
test -s "$tmp/private"
objcopy --localize-symbols="$tmp/private" "$tmp/authority.raw.o" "$tmp/efi/build/k-authority.o"
# shellcheck disable=SC2086
cc $flags -c "$here/wb_publish.c" -o "$tmp/efi/build/k-wb-publication.o"
common=$(sed -n 's/^KCOMMON = //p' "$repo/efi/Makefile" | sed 's@build/k-memory.o@build/k-authority.o@')
test -n "$common"
# Build the real standard kernel target from fresh sources, replacing PMM only.
# No final install, ESP copy, boot, or default repository build occurs.
make -C "$tmp/efi" EFI_INC="$EFI_INC" CC='gcc -fno-lto' \
 LD="ld -Map=$out/kernel.map" KCOMMON="$common build/k-wb-publication.o" \
 build/krnl.elf > "$out/build.log" 2>&1
cp "$tmp/efi/build/krnl.elf" "$out/authority-link-proof.elf"
python3 "$here/final_link_check.py" "$tmp/efi/build" "$out"
python3 "$here/capture_proof.py" "$out"
cc -std=c11 -Wall -Wextra -Werror "$here/wb_publish.c" "$here/wb_publish_test.c" -o "$tmp/fence_test"
"$tmp/fence_test"
echo "isolated final ELF link gates: PASS; $out/authority-link-proof.elf (NOT BOOTED)"
