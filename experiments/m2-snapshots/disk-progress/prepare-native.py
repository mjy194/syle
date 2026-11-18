#!/usr/bin/env python3
"""Prepare fresh controls and one exclusive positive run from recorded media."""
from pathlib import Path
import argparse,hashlib,json,shutil,sys
p=argparse.ArgumentParser();p.add_argument('candidate',type=Path);p.add_argument('--ledger',type=int,required=True);p.add_argument('--linux-console',action='store_true');a=p.parse_args();root=a.candidate.resolve()
base=Path('/home/javion/share/research/syle/efi/build/firmware-halt-20261114/reproduction/prepare-native-stage.py')
s=base.read_text().replace('krnl-firmware-halt.elf','krnl-firmware-disk.elf');sys.argv=['prepare','native','131072',str(a.ledger),str(root)];exec(compile(s,str(base),'exec'))
stage=root/'native';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for kind in ['nohv','positive-once']:
 d=stage/kind;shutil.copy2(root/'source/efi/build/firmware-disk-BOOTX64.EFI',d/'esp/EFI/BOOT/BOOTX64.EFI');shutil.copy2(root/'disk.img',d/'esp/syle/disk.img')
 (d/'media.sha256').write_text(''.join(sha(f)+'  '+str(f.relative_to(d))+'\n' for f in sorted(d.rglob('*')) if f.is_file() and f.name!='media.sha256'))
accepted=Path('/home/javion/share/research/syle/efi/build/firmware-console-20261117-accepted/native')
s=(accepted/'positive_once.py').read_text().replace(str(accepted),str(stage)).replace("commands=[b'ver\\r',b'echo SYLE_CONSOLE_OK_20261117\\r']","commands=[]")
if a.linux_console:
 s=s.replace('commands=[];last_prompt=0', "commands=[b'uname -s\\r',b'uname -r\\r',b'echo SYLE_LINUX_DISK_COMMAND_OK_20261118\\r'];last_prompt=0")
 s=s.replace("prompts=plain.count(b'Shell>')", "prompts=plain.count(b'\\n# ') if b'SYLE_LINUX_USERSPACE_READY' in plain else 0")
(stage/'positive_once.py').write_text(s);shutil.copy2(accepted/'installed.sha256',stage/'installed.sha256')
