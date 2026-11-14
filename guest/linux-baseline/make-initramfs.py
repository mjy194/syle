#!/usr/bin/env python3
"""Deterministic, uncompressed newc initramfs. No root/cpio required.
This is a NEW fixture, never a replacement for the historical fixed hash.
"""
import argparse, hashlib, json, pathlib, stat, struct

def static_x64(path):
    data=path.read_bytes()
    if len(data)<64 or data[:6]!=b'\x7fELF\x02\x01' or struct.unpack_from('<H',data,18)[0]!=62:
        raise ValueError(f'{path}: expected little-endian x86-64 ELF')
    off=struct.unpack_from('<Q',data,32)[0]
    size,count=struct.unpack_from('<HH',data,54)
    if size<56 or off+size*count>len(data): raise ValueError('invalid ELF program headers')
    for i in range(count):
        if struct.unpack_from('<I',data,off+i*size)[0]==3: raise ValueError(f'{path}: dynamically linked ELF not allowed')
    return data

def record(name, mode, data, ino, rmajor=0,rminor=0):
    name=name.encode()+b'\0'
    fields=[ino,mode,0,0,1,0,len(data),0,0,rmajor,rminor,len(name),0]
    h=b'070701'+b''.join(f'{v:08x}'.encode() for v in fields)+name
    return h+b'\0'*((-len(h))%4)+data+b'\0'*((-len(data))%4)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--busybox',type=pathlib.Path,required=True)
    p.add_argument('--probe',type=pathlib.Path,required=True)
    p.add_argument('--output',type=pathlib.Path,required=True)
    a=p.parse_args()
    bb=static_x64(a.busybox); probe=static_x64(a.probe)
    init=b'''#!/bin/sh
export PATH=/bin:/sbin
mount -t proc proc /proc || exit 1
mount -t sysfs sysfs /sys || exit 1
mount -t devtmpfs devtmpfs /dev || exit 1
mkdir -p /tmp
printf 'SYLE_LINUX_USERSPACE_READY\\n'
while :; do setsid cttyhack /bin/sh -i; done
'''
    entries={}
    for n in ['bin','sbin','dev','proc','sys','tmp']:
        entries[n]=(stat.S_IFDIR|0o755,b'',0,0)
    entries['tmp']=(stat.S_IFDIR|0o1777,b'',0,0)
    for n,target in [('sh','busybox'),('mount','busybox'),('mkdir','busybox'),('setsid','busybox'),('cttyhack','busybox'),('sleep','busybox'),('cat','busybox'),('uname','busybox')]:
        entries['bin/'+n]=(stat.S_IFLNK|0o777,target.encode(),0,0)
    entries['bin/busybox']=(stat.S_IFREG|0o755,bb,0,0)
    entries['bin/baseline-probe']=(stat.S_IFREG|0o755,probe,0,0)
    entries['init']=(stat.S_IFREG|0o755,init,0,0)
    entries['dev/console']=(stat.S_IFCHR|0o600,b'',5,1)
    entries['dev/null']=(stat.S_IFCHR|0o666,b'',1,3)
    out=bytearray()
    for i,(n,(m,d,major,minor)) in enumerate(sorted(entries.items()),1):
        out+=record(n,m,d,i,major,minor)
    out+=record('TRAILER!!!',0,b'',len(entries)+1)
    out+=b'\0'*((-len(out))%512)
    # Exclusive output prevents accidental overwriting of an accepted fixture.
    with a.output.open('xb') as f:f.write(out)
    manifest={'format':'syle-linux-baseline-v1','archive_sha256':hashlib.sha256(out).hexdigest(),
        'busybox_sha256':hashlib.sha256(bb).hexdigest(),'probe_sha256':hashlib.sha256(probe).hexdigest(),
        'note':'new fixture; not the historical managed-guest initramfs'}
    print(json.dumps(manifest,sort_keys=True,indent=2))
if __name__=='__main__':main()
