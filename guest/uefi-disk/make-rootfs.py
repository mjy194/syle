#!/usr/bin/env python3
"""Build an unmounted ext2 root. Reuse only the pinned static BusyBox binary."""
from pathlib import Path
import argparse,hashlib,json,os,stat,subprocess
p=argparse.ArgumentParser();p.add_argument('--initramfs',type=Path,required=True);p.add_argument('--probe',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=False);tree=a.output/'tree';tree.mkdir();archive=a.initramfs.read_bytes();off=0;busybox=None
while off<len(archive):
 assert archive[off:off+6]==b'070701';fields=[int(archive[off+6+i*8:off+14+i*8],16) for i in range(13)];size=fields[6];ns=fields[11];name=archive[off+110:off+110+ns-1].decode();start=(off+110+ns+3)&~3
 if name=='bin/busybox':busybox=archive[start:start+size]
 if name=='TRAILER!!!':break
 off=(start+size+3)&~3
assert busybox and busybox[:4]==b'\x7fELF'
for d in ['bin','sbin','dev','proc','sys','tmp','etc','persist']:(tree/d).mkdir()
bb=tree/'bin/busybox';bb.write_bytes(busybox);bb.chmod(0o755)
probe=tree/'bin/persistctl';probe.write_bytes(a.probe.read_bytes());probe.chmod(0o755)
for app in ['sh','mount','umount','mkdir','setsid','cttyhack','sync','reboot','cat','uname','sha256sum','stat','df','sleep']:(tree/'bin'/app).symlink_to('busybox')
init=tree/'sbin/init';init.write_text('''#!/bin/sh
export PATH=/bin:/sbin
mount -t proc proc /proc || exec /bin/sh
mount -t sysfs sysfs /sys || exec /bin/sh
mount -t devtmpfs devtmpfs /dev 2>/dev/null
printf 'SYLE_DISK_ROOT_READY\\n'
cat /proc/mounts
while :; do setsid cttyhack /bin/sh -i; done
''');init.chmod(0o755);(tree/'tmp').chmod(0o1777)
(tree/'etc/root-origin').write_text('SYLE_EXT2_DISK_ROOT_20261118\n')
image=a.output/'root.ext2';image.touch();image.write_bytes(b'')
with image.open('r+b') as f:f.truncate(55262*512)
env=dict(os.environ,E2FSPROGS_FAKE_TIME='1731801600',SOURCE_DATE_EPOCH='1731801600')
with (a.output/'build.log').open('w') as log:
 subprocess.run(['mke2fs','-F','-t','ext2','-b','1024','-I','128','-m','0','-O','none,filetype,sparse_super,large_file','-U','2df45771-a7e3-4ee8-855f-eb23aec45100','-L','SYLE_ROOT','-d',str(tree),str(image)],check=True,env=env,stdout=log,stderr=subprocess.STDOUT)
 subprocess.run(['debugfs','-w','-R','cd /dev',str(image)],check=True,env=env,stdout=log,stderr=subprocess.STDOUT)
 # debugfs commands share a single invocation so relative node paths are in /dev.
 commands=a.output/'devices.debugfs';commands.write_text('cd /dev\nmknod console c 5 1\nmknod null c 1 3\n')
 subprocess.run(['debugfs','-w','-f',str(commands),str(image)],check=True,env=env,stdout=log,stderr=subprocess.STDOUT)
 subprocess.run(['e2fsck','-fn',str(image)],check=True,env=env,stdout=log,stderr=subprocess.STDOUT)
record={'root_bytes':image.stat().st_size,'root_sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'busybox_sha256':hashlib.sha256(busybox).hexdigest(),'root_uuid':'2df45771-a7e3-4ee8-855f-eb23aec45100','init_sha256':hashlib.sha256(init.read_bytes()).hexdigest()};(a.output/'rootfs.json').write_text(json.dumps(record,indent=2));print(json.dumps(record))
