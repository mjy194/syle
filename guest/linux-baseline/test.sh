#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
t=$(mktemp -d); trap 'rm -rf "$t"' EXIT HUP INT TERM
"$here/build.sh" "$t/probe"
# Program fixed scratch name: never disturb a preexisting test file.
test ! -e /tmp/syle-program-test
"$t/probe" check | grep '^SYLE_PROGRAM_OK$'
set +e
"$t/probe" exit37 > "$t/output"; r=$?
set -e
test "$r" = 37
set +e
"$t/probe" unexpected > "$t/usage"; r=$?
set -e
test "$r" = 2; grep '^usage:' "$t/usage"
grep '^SYLE_PROGRAM_EXIT37$' "$t/output"
# Archive encoder test uses the static probe as structural stand-in only.
# It is NOT a functional BusyBox or bootable guest validation.
for n in a b; do
 python3 "$here/make-initramfs.py" --busybox "$t/probe" --probe "$t/probe" --output "$t/$n.cpio" > "$t/$n.json"
done
cmp "$t/a.cpio" "$t/b.cpio"; cmp "$t/a.json" "$t/b.json"
python3 - "$t/a.cpio" <<'PY'
import sys,stat
b=open(sys.argv[1],'rb').read(); p=0; entries={}
while True:
 assert b[p:p+6]==b'070701'
 f=[int(b[p+6+i*8:p+14+i*8],16) for i in range(13)]
 s=p+110; name=b[s:s+f[11]-1].decode(); p=(s+f[11]+3)&~3
 data=b[p:p+f[6]]; p=(p+f[6]+3)&~3
 if name=='TRAILER!!!':break
 assert name not in entries and f[2:4]==[0,0] and f[5]==0
 entries[name]=(f,data)
assert entries['dev/console'][0][9:11]==[5,1]
assert stat.S_ISCHR(entries['dev/console'][0][1])
assert entries['bin/sh'][1]==b'busybox'
assert b'SYLE_LINUX_USERSPACE_READY' in entries['init'][1]
assert len(b)%512==0
print('PASS deterministic newc encoder, metadata and contents (host only)')
PY
# The archive creator must not overwrite a previous output.
if python3 "$here/make-initramfs.py" --busybox "$t/probe" --probe "$t/probe" --output "$t/a.cpio" >/dev/null 2>&1; then exit 1; fi
python3 "$here/test-inputs.py"
printf 'PASS independent static program and fixture tooling (host only)\n'
