#!/usr/bin/env python3
"""Extend the exact accepted disk runtime, preserving its frozen source graph."""
from pathlib import Path
import argparse,hashlib,json,shutil,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'disk-progress'))
from prepare import once
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
repo=Path(__file__).resolve().parents[3];base=repo/'efi/build/firmware-disk-accepted-20261118';dest=a.output/'source'
a.output.mkdir(parents=True,exist_ok=False)
for line in (base/'source.sha256').read_text().splitlines():
 h,name=line.split(None,1);f=base/'source'/name.strip();assert hashlib.sha256(f.read_bytes()).hexdigest()==h
 target=dest/name.strip();target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,target)
files=['krnl/firmware_persistent_observe.inc','krnl/firmware_disk_observe.inc','krnl/pic_master_04.c','krnl/pic_master_04.h','efi/firmware-persistent.mk']+[str(f.relative_to(repo)) for f in (repo/'krnl/virtio_blk').iterdir() if f.is_file()]
for name in files:
 target=dest/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(repo/name,target)
f=dest/'krnl/firmware_ovmf_quiet_observe.c';s=f.read_text().replace('#include "firmware_disk_observe.inc"','#include "firmware_persistent_observe.inc"')
s=once(s,'if(console_matches("\\nSYLE_LINUX_DISK_COMMAND_OK_20261118\\r\\n") && console_matches("\\nLinux\\r\\n") && console_matches("\\n6.12.0-syle-efi1\\r\\n") && console_matches("\\n# ")>=4)', 'if(persistent_reboot)')
s=once(s,'  if(port>=0x3f8', '  if(port==0x64||port==0xcf9)return persistent_reset(rip,q,(unsigned)len);\n  if(port>=0x3f8') if '  if(port>=0x3f8' in s else once(s,'  if(port==0x80||','  if(port==0x64||port==0xcf9)return persistent_reset(rip,q,(unsigned)len);\n  if(port==0x80||')
s=s.replace('FPM_DELAY_EXIT_CAP==131072,','FPM_DELAY_EXIT_CAP==131072 || FPM_DELAY_EXIT_CAP==1048576,')
s=once(s,'  if(port==0x64||port==0xcf9)', '  if(port==0xcfb)return persistent_config_probe(rip,q,(unsigned)len);\n  if(port==0x64||port==0xcf9)')
s=s.replace('UINT64_C(300000000)','UINT64_C(1200000000)').replace('reference_seconds=30','reference_seconds=120').replace('observation_time_budget=30s','observation_time_budget=120s')
s=s.replace('console_sample_quiet=console_state.command && (observed&255)!=0;', 'console_sample_quiet=console_state.command && (observed&1023)!=0;')
s=once(s,' log_text("OVMF.DISK terminal bar=");',' log_text("OVMF.PERSIST stats reads=");log_hex(disk_state.reads);log_text(" writes=");log_hex(disk_state.writes);log_text(" flushes=");log_hex(disk_state.flushes);log_text(" irq5=");log_hex(disk_irq5_receipts);log_text(" reboot=");log_hex(persistent_reboot);log_text("\\n");\n log_text("OVMF.DISK terminal bar=");')
f.write_text(s)
f=dest/'efi/firmware-disk-runtime.mk';f.write_text('build/fdisk-observer.o: KFLAGS += -DFPM_DELAY_EXIT_CAP=1048576 -DSYLE_FIRMWARE_DISK_PROBE\n')
(a.output/'source.sha256').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+str(f.relative_to(dest))+'\n' for f in sorted(dest.rglob('*')) if f.is_file()))
(a.output/'preparation.json').write_text(json.dumps({'base':str(base),'guest_disk':'00:03.0 INTx IRQ5','backing':'fixed L1 PIIX3 PIO, no DMA, serial SYLE-PERSIST-0001, 64MiB','budget':{'exits':1048576,'reference_seconds':120,'trace_bytes':2097152}},indent=2))
