#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:?usage: build.sh OUTPUT_DIRECTORY}
mkdir -p "$out"
${CC:-cc} -c -m64 -ffreestanding -fno-pie "$here/reset.S" -o "$out/reset.o"
${LD:-ld} -m elf_x86_64 -nostdlib -T "$here/link.ld" "$out/reset.o" -o "$out/reset.elf"
${OBJCOPY:-objcopy} -O binary "$out/reset.elf" "$out/reset.bin"
python3 - "$out/reset.bin" <<'PY'
import sys,pathlib
b=pathlib.Path(sys.argv[1]).read_bytes()
assert len(b)==65536
assert b[0xfff0:0xfff3]==bytes.fromhex('e90d00'),b[0xfff0:0xfff3].hex()
for marker in (0x16,0x32,0x64):assert bytes([0xb0,marker,0xe6,0xe9]) in b
print('firmware-probe: 64KiB/reset placement/three mode markers PASS (build only)')
PY
