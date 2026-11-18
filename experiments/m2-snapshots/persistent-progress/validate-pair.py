#!/usr/bin/env python3
"""Two fresh boots: write+fsync+reset, same backing file, read+verify+reset.
Requires a newly prepared/built candidate with disk.img. Never reuses a run.
"""
from pathlib import Path
import argparse,hashlib,json,secrets,shutil,subprocess,sys
repo=Path(__file__).resolve().parents[3];sys.path.insert(0,str(repo/'tools'))
from analyze_persistent_disk import phase_check,offline,analyze
p=argparse.ArgumentParser();p.add_argument('candidate',type=Path);p.add_argument('--linux-config',type=Path,required=True);p.add_argument('--ledger',type=int,required=True);a=p.parse_args();root=a.candidate.resolve()
assert not any((root/n).exists() for n in ['native','backing','reboot','experiment.json'])
seed=root/'disk.img';assert seed.stat().st_size==67108864
config=a.linux_config.read_text();assert 'CONFIG_INITRAMFS_SOURCE=""' in config and 'CONFIG_EXT2_FS=y' in config
(root/'inputs').mkdir();shutil.copy2(a.linux_config,root/'inputs/linux.config')
(root/'backing').mkdir();backing=root/'backing/disk.raw';shutil.copy2(seed,backing);token=secrets.token_hex(16);assert token.encode() not in seed.read_bytes()
exp={'token':token,'seed_sha256':hashlib.sha256(seed.read_bytes()).hexdigest(),'file_sha256':hashlib.sha256((token+'\n').encode()).hexdigest(),'root_partition':{'first_lba':75776,'last_lba':131038},'model':'fresh QEMU/L1/L2 after architectural guest reset; same backing inode; no RAM snapshot/restore'};(root/'experiment.json').write_text(json.dumps(exp,indent=2))
def prepare(run,phase,count):
 subprocess.run([sys.executable,str(Path(__file__).with_name('prepare-native.py')),str(run),'--backing',str(backing),'--phase',phase,'--token',token,'--ledger',str(count)],check=True)
def execute(run,phase):
 subprocess.run([sys.executable,str(run/'native/validate_once.py')],check=True)
 stage=run/'native/positive-once';res=json.loads((stage/'result.json').read_text());obj,plain=phase_check((stage/'serial.log').read_bytes(),json.loads((stage/'input.json').read_text()),res,phase,token);(stage/'console.txt').write_bytes(plain)
 shutil.copy2(backing,root/f'{phase}-after.raw');offline(root/f'{phase}-after.raw',token,res['backing_after_sha256'],root/'verification'/phase);return res
prepare(root,'write',a.ledger);first=execute(root,'write')
reboot=root/'reboot';reboot.mkdir();dest=reboot/'source'
for line in (root/'source.sha256').read_text().splitlines():
 h,name=line.split(None,1);src=root/'source'/name.strip();assert hashlib.sha256(src.read_bytes()).hexdigest()==h
 dst=dest/name.strip();dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
for name in ['source.sha256','preparation.json','disk.img','disk.json']:shutil.copy2(root/name,reboot/name)
build=dest/'efi/build';build.mkdir()
for name in ['krnl-firmware-disk.elf','firmware-disk-BOOTX64.EFI']:shutil.copy2(root/'source/efi/build'/name,build/name)
assert hashlib.sha256(backing.read_bytes()).hexdigest()==first['backing_after_sha256']
prepare(reboot,'read',a.ledger+1);execute(reboot,'read');print(json.dumps(analyze(root),indent=2))
