#!/usr/bin/env python3
"""Deterministic GPT/FAT32 ESP, no mount or host block device access."""
import argparse,hashlib,json,struct,uuid,zlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--efi',type=Path,required=True);p.add_argument('--linux',type=Path);p.add_argument('--rootfs',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
b=bytearray(64*1024*1024);sectors=len(b)//512;first=2048;last=75775 if a.rootfs else sectors-34;total=last-first+1;fat_sectors=1000;data_sector=first+32+2*fat_sectors
struct.pack_into('<B3sB3sII',b,446,0,b'\0\2\0',0xee,b'\xff'*3,1,sectors-1);b[510:512]=b'\x55\xaa'
entries=bytearray(128*128);entries[:16]=uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b').bytes_le;entries[16:32]=uuid.UUID('9f93b65a-3a91-4f62-94b6-04041679211e').bytes_le;struct.pack_into('<QQQ',entries,32,first,last,0);name='SYLE ESP'.encode('utf-16-le');entries[56:56+len(name)]=name
if a.rootfs:
 root_start=last+1;root_end=sectors-34;root=a.rootfs.read_bytes();assert len(root)<=(root_end-root_start+1)*512
 entries[128:144]=uuid.UUID('0fc63daf-8483-4772-8e79-3d69d8477de4').bytes_le
 entries[144:160]=uuid.UUID('baafdd52-68ef-4f4f-b1e5-0a7cc117c4d5').bytes_le
 struct.pack_into('<QQQ',entries,160,root_start,root_end,0)
 name='SYLE ROOT'.encode('utf-16-le');entries[184:184+len(name)]=name
 b[root_start*512:root_start*512+len(root)]=root
for current,backup,entry_lba in [(1,sectors-1,2),(sectors-1,1,sectors-33)]:
 h=bytearray(512);h[:8]=b'EFI PART';struct.pack_into('<IIIIQQQQ',h,8,0x10000,92,0,0,current,backup,34,sectors-34);h[56:72]=uuid.UUID('3f6763f0-38ed-4cb3-bb73-3a1ba9bf4ac5').bytes_le;struct.pack_into('<QIII',h,72,entry_lba,128,128,zlib.crc32(entries));struct.pack_into('<I',h,16,zlib.crc32(h[:92]));b[current*512:(current+1)*512]=h;b[entry_lba*512:entry_lba*512+len(entries)]=entries
v=bytearray(512);v[:11]=b'\xeb\x58\x90SYLEFAT ';struct.pack_into('<HBHBHHBHHHII',v,11,512,1,32,2,0,0,0xf8,0,63,255,first,total);struct.pack_into('<IHHIHH',v,36,fat_sectors,0,0,2,1,6);v[64]=0x80;v[66]=0x29;struct.pack_into('<I',v,67,0x53594c45);v[71:82]=b'SYLE ESP   ';v[82:90]=b'FAT32   ';v[510:512]=b'\x55\xaa'
b[first*512:(first+1)*512]=v;b[(first+6)*512:(first+7)*512]=v
fs=bytearray(512);struct.pack_into('<I',fs,0,0x41615252);struct.pack_into('<III',fs,484,0x61417272,0xffffffff,0xffffffff);struct.pack_into('<I',fs,508,0xaa550000);b[(first+1)*512:(first+2)*512]=fs
fat=bytearray(fat_sectors*512);next_cluster=2

def allocate(payload):
 global next_cluster
 n=max(1,(len(payload)+511)//512);start=next_cluster;next_cluster+=n
 assert next_cluster<(total-32-2*fat_sectors)+2
 for i in range(n):
  c=start+i;struct.pack_into('<I',fat,c*4,c+1 if i+1<n else 0x0fffffff)
  offset=(data_sector+c-2)*512;piece=payload[i*512:(i+1)*512];b[offset:offset+len(piece)]=piece
 return start

def entry(name,attr,c,size=0):
 e=bytearray(32);e[:11]=name.encode();e[11]=attr;struct.pack_into('<H',e,20,c>>16);struct.pack_into('<H',e,26,c&65535);struct.pack_into('<I',e,28,size);return e
root=allocate(b'');efi=allocate(b'');boot=allocate(b'');payload=a.efi.read_bytes();bootfile=allocate(payload);test=b'SYLE_FAT_DISK_READ_VERIFIED\n';testfile=allocate(test)
boot_entries=entry('.          ',16,boot)+entry('..         ',16,efi)+entry('BOOTX64 EFI',32,bootfile,len(payload))
if a.linux:
 kernel=a.linux.read_bytes();cluster=allocate(kernel);boot_entries+=entry('LINUX   EFI',32,cluster,len(kernel))
for c,payload in [(root,entry('EFI        ',16,efi)+entry('SYLE    TXT',32,testfile,len(test))),(efi,entry('.          ',16,efi)+entry('..         ',16,root)+entry('BOOT       ',16,boot)),(boot,boot_entries)]:
 off=(data_sector+c-2)*512;b[off:off+len(payload)]=payload
struct.pack_into('<II',fat,0,0x0ffffff8,0xffffffff)
for i in range(2):off=(first+32+i*fat_sectors)*512;b[off:off+len(fat)]=fat
with a.output.open('xb') as f:f.write(b)
print(json.dumps({'sha256':hashlib.sha256(b).hexdigest(),'bytes':len(b),'efi_sha256':hashlib.sha256(a.efi.read_bytes()).hexdigest(),'linux_sha256':hashlib.sha256(a.linux.read_bytes()).hexdigest() if a.linux else None,'allocated_clusters':next_cluster-2},indent=2))
