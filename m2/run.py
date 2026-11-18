"""Run the manual M2 kernel with a persistent disk and local serial console."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('build',type=Path)
p.add_argument('run_directory',type=Path,help='new private directory for this launch')
p.add_argument('--seed',type=Path,required=True)
p.add_argument('--disk',type=Path,required=True,help='persistent raw disk, created from seed on first use')
p.add_argument('--vars',type=Path,required=True)
p.add_argument('--socket',action='store_true',help='use serial.sock for an external terminal')
p.add_argument('--check',action='store_true',help='run the serial acceptance check and shut down QEMU')
p.add_argument('--idle',type=int,default=130,help='seconds of guest idle in --check mode')
p.add_argument('--display',choices=('none','gtk'),default='none')
a=p.parse_args()
os.umask(0o077)
out=a.run_directory.resolve()
out.mkdir(exist_ok=False)
disk=a.disk.absolute()
if disk.is_symlink() or not a.seed.is_file() or a.seed.stat().st_size!=67108864:
    p.error('requires a regular 64 MiB seed and a non-symlink disk')
if not disk.exists():
    with disk.open('xb') as target, a.seed.open('rb') as source:
        shutil.copyfileobj(source,target)
if not disk.is_file() or disk.samefile(a.seed) or disk.stat().st_size!=67108864 or ',' in str(disk):
    p.error('invalid writable backing file')
esp=out/'esp'
(esp/'EFI/BOOT').mkdir(parents=True)
(esp/'syle').mkdir()
built=a.build/'source/efi/build'
shutil.copyfile(built/'firmware-disk-BOOTX64.EFI',esp/'EFI/BOOT/BOOTX64.EFI')
shutil.copyfile(built/'krnl-firmware-disk.elf',esp/'syle/krnl.elf')
shutil.copyfile(built/'firmware-mgmt.elf',esp/'syle/mgmt.elf')
shutil.copyfile(a.seed,esp/'syle/disk.img')
(esp/'syle/boot.cfg').write_text('default=normal\ntimeout=0\ntitle=Syle\n')
shutil.copyfile(a.vars,out/'vars.fd')
cmd=['qemu-system-x86_64','-machine','q35,accel=kvm','-cpu','host,hv-time=on','-smp','1','-m','512M',
     '-drive','if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd',
     '-drive','if=pflash,format=raw,file=vars.fd','-drive','if=none,id=esp,format=raw,readonly=on,file=fat:esp',
     '-device','virtio-blk-pci,drive=esp,bootindex=1','-nic','none','-display',a.display,'-monitor','none',
     '-serial','unix:serial.sock,server=on,wait=on' if a.socket or a.check else 'stdio',
     '-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-no-reboot',
     '-drive','if=none,id=persist-backend,format=raw,cache=writeback,aio=threads,file='+str(disk),
     '-device','piix3-ide,id=persist-ide,addr=05.0',
     '-device','ide-hd,id=persist-drive,bus=persist-ide.0,unit=0,drive=persist-backend,serial=SYLE-PERSIST-0001,bootindex=2']
(out/'command.json').write_text(json.dumps(cmd,indent=2))
with subprocess.Popen(cmd, cwd=out) as vm:
    try:
        if a.check:
            subprocess.run([sys.executable, str(Path(__file__).with_name('check_manual.py')),
                            str(out), '--idle', str(a.idle)], check=True)
        status = vm.wait(timeout=15 if a.check else None)
    finally:
        if vm.poll() is None:
            vm.terminate()
            try:
                vm.wait(timeout=5)
            except subprocess.TimeoutExpired:
                vm.kill()
raise SystemExit(0 if status == 33 else status or 1)
