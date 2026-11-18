#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,shutil
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();repo=Path(__file__).resolve().parents[3];base=repo/'efi/build/managed-firmware-remount-20261118';dest=a.output/'source';a.output.mkdir(parents=True,exist_ok=False)
for line in (base/'source.sha256').read_text().splitlines():
 h,name=line.split(None,1);name=name.strip();f=base/'source'/name;assert hashlib.sha256(f.read_bytes()).hexdigest()==h
 t=dest/name;t.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,t)
files=['krnl/main.c','krnl/firmware_managed.inc','krnl/firmware_fwcfg.c','krnl/firmware_fwcfg_transaction.c','krnl/firmware_fwcfg_transaction.h','krnl/firmware_ramfb.c','krnl/firmware_ramfb.h','krnl/ramfb_config.c','krnl/ramfb_config.h','krnl/firmware_display.c','krnl/firmware_display.h','efi/firmware-graphics.mk','user/mgmt/firmware_lifecycle.h','guest/uefi-disk/boot.c','guest/uefi-disk/fb-probe.c','guest/uefi-disk/build.sh']
for name in files:
 t=dest/name;t.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/name,t)
shutil.copy2(Path(__file__).with_name('firmware_lifecycle.h'),dest/'user/mgmt/firmware_lifecycle.h')
(a.output/'source.sha256').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+str(f.relative_to(dest))+'\n' for f in sorted(dest.rglob('*')) if f.is_file()))
(a.output/'preparation.json').write_text(json.dumps({'base':str(base),'profile':'Ring3 + ramfb DMA/GOP + bounded host scanout + writable disk'},indent=2))
