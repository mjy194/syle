#!/usr/bin/env python3
"""Separate EFI-stub Linux fixture; preserves all direct-loader inputs."""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'linux-rebuild'))
from recipe import SOURCE_SHA256,digest,apply_compat
p=argparse.ArgumentParser();p.add_argument('--source-tar',type=Path,required=True);p.add_argument('--initramfs',type=Path);p.add_argument('--disk-root',action='store_true');p.add_argument('--output',type=Path,required=True);a=p.parse_args()
assert digest(a.source_tar)==SOURCE_SHA256
assert a.disk_root or a.initramfs
root=a.output.resolve();root.mkdir(parents=True,exist_ok=False);source=root/'linux-6.12';obj=root/'objects';obj.mkdir()
archive=None
if not a.disk_root:
 archive=root/'initramfs.cpio';shutil.copy2(a.initramfs,archive)
base=Path(__file__).resolve().parents[1]/'linux-rebuild/config.fragment'
fragment=base.read_text().replace('CONFIG_PCI=n','CONFIG_PCI=y').replace('CONFIG_ACPI=n','CONFIG_ACPI=y').replace('CONFIG_SERIAL_8250_NR_UARTS=1','CONFIG_SERIAL_8250_NR_UARTS=4').replace('CONFIG_SERIAL_8250_RUNTIME_UARTS=1','CONFIG_SERIAL_8250_RUNTIME_UARTS=4').replace('CONFIG_LOCALVERSION="-syle-rebuild1"','CONFIG_LOCALVERSION="-syle-efi1"').replace('CONFIG_INITRAMFS_SOURCE=""',f'CONFIG_INITRAMFS_SOURCE="{archive}"' if archive else 'CONFIG_INITRAMFS_SOURCE=""')
fragment+='\nCONFIG_BLOCK=y\nCONFIG_BLK_DEV=y\nCONFIG_VIRTIO_MENU=y\nCONFIG_EFI=y\nCONFIG_EFI_STUB=y\nCONFIG_EFI_MIXED=n\nCONFIG_SERIAL_8250_PCI=y\nCONFIG_VIRTIO=y\nCONFIG_VIRTIO_PCI=y\nCONFIG_VIRTIO_PCI_LEGACY=y\nCONFIG_VIRTIO_BLK=y\nCONFIG_RD_GZIP=y\n'
if a.disk_root:
 fragment=fragment.replace('CONFIG_LOCALVERSION="-syle-efi1"','CONFIG_LOCALVERSION="-syle-root1"').replace('CONFIG_SERIAL_8250_PCI=y','CONFIG_SERIAL_8250_PCI=n').replace('CONFIG_SERIAL_8250_NR_UARTS=4','CONFIG_SERIAL_8250_NR_UARTS=1').replace('CONFIG_SERIAL_8250_RUNTIME_UARTS=4','CONFIG_SERIAL_8250_RUNTIME_UARTS=1')
 fragment+='\nCONFIG_EXT2_FS=y\nCONFIG_DEVTMPFS_MOUNT=y\n'
(root/'config.fragment').write_text(fragment)
env=dict(os.environ,ARCH='x86_64',KCONFIG_ALLCONFIG=str(root/'config.fragment'),KBUILD_BUILD_USER='syle',KBUILD_BUILD_HOST='efi',KBUILD_BUILD_TIMESTAMP='2024-11-17 00:00:00 UTC',KBUILD_BUILD_VERSION='1',SOURCE_DATE_EPOCH='1731801600',LC_ALL='C',TZ='UTC')
with (root/'build.log').open('w') as log:
 def run(cmd):log.write(repr(cmd)+'\n');log.flush();subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 run(['tar','-xJf',str(a.source_tar.resolve()),'-C',str(root),'--no-same-owner','--no-same-permissions']);compat=apply_compat(source)
 stub=source/'drivers/firmware/efi/libstub/Makefile';old=stub.read_bytes();needle=b'   -Os -DDISABLE_BRANCH_PROFILING';assert old.count(needle)==1
 stub.write_bytes(old.replace(needle,b'   -std=gnu11 -Os -DDISABLE_BRANCH_PROFILING'))
 compat['efi_stub']={'before':hashlib.sha256(old).hexdigest(),'after':digest(stub)}
 args=['make','-C',str(source),'O='+str(obj),'CC=gcc','HOSTCC=gcc','LD=ld']
 run(args+['allnoconfig'])
 config=(obj/'.config').read_text()
 for key in ['CONFIG_EFI=y','CONFIG_EFI_STUB=y','CONFIG_PCI=y','CONFIG_VIRTIO_BLK=y']:assert key in config,key
 run(args+['-j2','bzImage'])
shutil.copy2(obj/'arch/x86/boot/bzImage',root/'LINUX.EFI');shutil.copy2(obj/'.config',root/'resolved.config')
record={'source_sha256':SOURCE_SHA256,'kernel_sha256':digest(root/'LINUX.EFI'),'initramfs_sha256':digest(archive) if archive else None,'disk_root':a.disk_root,'config_sha256':digest(root/'resolved.config'),'compatibility':compat,'tools':{t:{'path':shutil.which(t),'sha256':digest(shutil.which(t))} for t in ['gcc','ld','objcopy','make']},'status':'built-not-runtime-validated'}
(root/'build.json').write_text(json.dumps(record,indent=2));print(json.dumps(record))
