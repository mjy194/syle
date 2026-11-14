#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../.." && pwd)
: "${MB_INPUTS:=/workspace/shared/clock-evidence/inputs}"
: "${EFI_INC:=/workspace/shared/syle-deps/root/usr/include/efi}"
test -r "$EFI_INC/efi.h"
if test "${EXCEPTION_GATE_LINK:-0}" = 1 && test "${NMI_GUARD_LINK:-0}" != 1; then
 echo 'EXCEPTION_GATE_LINK requires isolated NMI_GUARD_LINK tree' >&2;exit 1
fi
# The caller can retain the artifact; no original build directory is modified.
out=${1:?usage: managed_final_link.sh NEW_OUTPUT_DIRECTORY}
if test -e "$out"; then echo 'choose a new output directory' >&2; exit 1; fi
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/efi/build"
if test "${NMI_GUARD_LINK:-0}" = 1; then
 mkdir -p "$tmp/krnl"
 for f in "$repo"/krnl/*; do name=$(basename "$f");test "$name" = ring3.c && continue
  if test "${PREPARE_CONTEXT_LINK:-0}" = 1 && test "$name" = linux_guest_manager.c; then continue; fi
  ln -s "$f" "$tmp/krnl/$name"; done
else
 ln -s "$repo/krnl" "$tmp/krnl"
fi
ln -s "$repo/include" "$tmp/include"
ln -s "$repo/boot" "$tmp/boot"
ln -s "$repo/guest" "$tmp/guest"
ln -s "$repo/user" "$tmp/user"
ln -s "$repo/efi/Makefile" "$tmp/efi/Makefile"
if test "${NMI_GUARD_LINK:-0}" = 1; then
 mkdir -p "$tmp/efi/src"
 for f in "$repo"/efi/src/*; do
  name=$(basename "$f");test "$name" != exceptions.c || continue
  if test "${EXCEPTION_GATE_LINK:-0}" = 1; then
   case "$name" in exceptions.S|linux_exceptions.S) continue;; esac
  fi
  ln -s "$f" "$tmp/efi/src/$name"
 done
 probe='';test "${NMI_PROBE_LINK:-0}" != 1 || probe='--probe'
 python3 "$here/nmi_guard_candidate.py" "$tmp" --patched $probe
 # Preserve exact candidate deltas alongside captured original sources.
 mkdir -p "$out/nmi-candidate"
 cp "$tmp/krnl/ring3.c" "$tmp/efi/src/exceptions.c" "$out/nmi-candidate/"
fi
if test "${NMI_GUARD_LINK:-0}" != 1; then ln -s "$repo/efi/src" "$tmp/efi/src"; fi
for f in "$repo"/efi/*.mk; do ln -s "$f" "$tmp/efi/$(basename "$f")"; done
if test "${EXCEPTION_GATE_LINK:-0}" = 1; then
 python3 "$here/exception_context_gate_candidate.py" "$tmp/efi/src/exceptions.c"
 python3 "$here/exception_marker_candidate.py" "$tmp/efi/src"
 mkdir -p "$out/context-candidate"
 cp "$tmp/efi/src/exceptions.c" "$tmp/efi/src/exceptions.S" "$tmp/efi/src/linux_exceptions.S" "$out/context-candidate/"
fi
if test "${PREPARE_CONTEXT_LINK:-0}" = 1; then
 test "${NMI_GUARD_LINK:-0}" = 1 || { echo 'PREPARE_CONTEXT_LINK requires private krnl tree';exit 1; }
 python3 "$here/prepare_context_candidate.py" "$repo/krnl/linux_guest_manager.c" "$tmp/krnl/linux_guest_manager.c"
 mkdir -p "$out/prepare-context-candidate"
 cp "$tmp/krnl/linux_guest_manager.c" "$here/prepare_context_observe.c" "$here/prepare_context_pause.S" "$out/prepare-context-candidate/"
fi
flags='-std=c11 -O1 -ffreestanding -fshort-wchar -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-stack-check -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-pic -fno-pie -mcmodel=small -DSYLE_KERNEL -fno-lto -Wall -Wextra -Werror'
# shellcheck disable=SC2086
cc $flags -c "$here/native_candidate.c" -o "$tmp/authority.raw.o"
nm -g --defined-only "$tmp/authority.raw.o" | awk '$3 ~ /^(pmm_raw_|pmm_owned_|vlm_)/ {print $3}' > "$tmp/private"
test -s "$tmp/private"
objcopy --localize-symbols="$tmp/private" "$tmp/authority.raw.o" "$tmp/efi/build/k-authority.o"
# shellcheck disable=SC2086
cc $flags -c "$here/wb_publish.c" -o "$tmp/efi/build/k-wb-publication.o"
extra_binding=''
if test "${PREPARE_CONTEXT_LINK:-0}" = 1; then
 cc $flags -c "$here/prepare_context_observe.c" -o "$tmp/efi/build/k-prepare-observe.o"
 cc $flags -c "$here/prepare_context_pause.S" -o "$tmp/efi/build/k-prepare-pause.o"
 extra_binding="$extra_binding build/k-prepare-observe.o build/k-prepare-pause.o"
fi
if test "${MANAGED_BOUNDED_LINK:-0}" = 1; then
 python3 "$here/managed_bounded_candidate.py" "$repo/krnl/linux_guest_role_binding.c" "$tmp/managed-bounded.c"
 python3 "$here/lgm_bounded_candidate.py" "$repo/krnl/linux_guest_memory.c" "$tmp/lgm-bounded.c"
 python3 "$here/boot_source_candidate.py" "$repo/krnl/linux_boot.c" "$tmp/boot-source.c"
 for unit in managed-bounded lgm-bounded boot-source; do
  cc $flags -I"$here" -I"$repo/krnl" -c "$tmp/$unit.c" -o "$tmp/efi/build/k-$unit.o"
  extra_binding="$extra_binding build/k-$unit.o"
 done
 for unit in bounded_span managed_bounded_refusal; do
  cc $flags -I"$here" -I"$repo/krnl" -c "$here/$unit.c" -o "$tmp/efi/build/k-$unit.o"
  extra_binding="$extra_binding build/k-$unit.o"
 done
 mkdir -p "$out/managed-bounded-candidate"
 cp "$tmp/managed-bounded.c" "$tmp/lgm-bounded.c" "$tmp/boot-source.c" "$here/managed_bounded_refusal.c" "$out/managed-bounded-candidate/"
fi
if test "${ROLE_BINDING_LINK:-0}" = 1; then
 # Static inclusion only; default manager still selects legacy callbacks.
 cc $flags -c "$repo/krnl/linux_guest_role_binding.c" -o "$tmp/efi/build/k-role-binding.o"
 extra_binding="$extra_binding build/k-role-binding.o"
fi
if test "${NATIVE_MAP_LINK:-0}" = 1; then
 for unit in native_map_candidate scoped_access session_authority create_context; do
  cc $flags -I"$EFI_INC" -I"$EFI_INC/x86_64" -c "$here/$unit.c" -o "$tmp/efi/build/k-$unit.o"
  extra_binding="$extra_binding build/k-$unit.o"
 done
fi
common=$(sed -n 's/^KCOMMON = //p' "$repo/efi/Makefile" | sed 's@build/k-memory.o@build/k-authority.o@')
test -n "$common"
# Build real managed-host-clock-24 target from fresh sources, PMM replacement only.
# No final install, ESP copy, boot, or default repository build occurs.
make -C "$tmp/efi" -f Makefile -f managed-host-clock.mk MB_INPUTS="$MB_INPUTS" HC_BITS=24 EFI_INC="$EFI_INC" CC='gcc -fno-lto' \
 LD="ld -Map=$out/kernel.map" KCOMMON="$common build/k-wb-publication.o $extra_binding" \
 build/krnl-managed-host-clock-24.elf > "$out/build.log" 2>&1
cp "$tmp/efi/build/krnl-managed-host-clock-24.elf" "$out/authority-link-proof.elf"
python3 "$here/final_link_check.py" "$tmp/efi/build" "$out"
python3 - "$out/manifest.json" "$MB_INPUTS" <<'PYMANIFEST'
import hashlib,json,pathlib,sys,os
p=pathlib.Path(sys.argv[1]);inputs=pathlib.Path(sys.argv[2]);m=json.loads(p.read_text());m['profile']='managed-host-clock-24, PMM authority replacement only; NOT BOOTED';m['optional_variants']={k:os.environ.get(k,'0') for k in ['ROLE_BINDING_LINK','NMI_GUARD_LINK','NATIVE_MAP_LINK','NMI_PROBE_LINK','EXCEPTION_GATE_LINK','MANAGED_BOUNDED_LINK','PREPARE_CONTEXT_LINK']};m['inputs']={n:hashlib.sha256((inputs/n).read_bytes()).hexdigest() for n in ['bzImage','initramfs.cpio','manifest.json']};p.write_text(json.dumps(m,indent=2)+'\n')
PYMANIFEST
python3 "$here/capture_proof.py" "$out" "$MB_INPUTS"
cc -std=c11 -Wall -Wextra -Werror "$here/wb_publish.c" "$here/wb_publish_test.c" -o "$tmp/fence_test"
"$tmp/fence_test"
echo "isolated managed-profile final ELF link gates: PASS; $out/authority-link-proof.elf (NOT BOOTED)"
