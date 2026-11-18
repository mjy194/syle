#!/usr/bin/env python3
"""New disk from pinned seed, actual GOP EFI probe and framebuffer Linux."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
p=argparse.ArgumentParser();p.add_argument('candidate',type=Path);p.add_argument('--kernel',type=Path,required=True);p.add_argument('--efi-prefix',required=True);a=p.parse_args();root=a.candidate.resolve();repo=Path(__file__).resolve().parents[3];src=root/'source';kernel=a.kernel.resolve()
(root/'inputs').mkdir(exist_ok=False)
for name,out in [('LINUX.EFI','LINUX.EFI'),('resolved.config','linux.config'),('build.json','linux-build.json')]:shutil.copy2(kernel/name,root/'inputs'/out)
seed=(repo/'efi/build/persistent-accepted-20261118/disk.img').read_bytes();assert hashlib.sha256(seed).hexdigest()=='638fe8556e27ecb9a196871e33e790e8ee96bd7ad7df258b07c2f060173db9f6'
fs=root/'root.ext2';fs.write_bytes(seed[75776*512:131039*512])
helper=root/'fbprobe';subprocess.run(['cc','-O2','-static','-nostdlib','-fno-pie','-no-pie','-fno-stack-protector','-fno-builtin','-mgeneral-regs-only','-mno-red-zone','-Wall','-Wextra','-Werror',str(src/'guest/uefi-disk/fb-probe.c'),'-o',str(helper)],check=True)
log=open(root/'disk-build.log','w')
for command in [f'write {helper} /bin/fbprobe','set_inode_field /bin/fbprobe mode 0100755']:subprocess.run(['debugfs','-w','-R',command,str(fs)],check=True,stdout=log,stderr=subprocess.STDOUT)
subprocess.run(['e2fsck','-fn',str(fs)],check=True,stdout=log,stderr=subprocess.STDOUT)
env=dict(os.environ,UEFI_DISK_CFLAGS='-DSYLE_DISK_LINUX -DSYLE_DISK_ROOT -DSYLE_DISK_GRAPHICS')
subprocess.run(['sh',str(src/'guest/uefi-disk/build.sh'),str(root/'disk-efi'),a.efi_prefix],env=env,check=True,stdout=log,stderr=subprocess.STDOUT)
with (root/'disk.json').open('w') as out:subprocess.run(['python3',str(repo/'guest/uefi-disk/make-disk.py'),'--efi',str(root/'disk-efi/BOOTX64.EFI'),'--linux',str(kernel/'LINUX.EFI'),'--rootfs',str(fs),'--output',str(root/'disk.img')],check=True,stdout=out)
print(root/'disk.img')
