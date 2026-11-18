#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--base',type=Path);a=p.parse_args()
repo=Path(__file__).resolve().parents[3];root=a.output.resolve();root.mkdir(exist_ok=False);obj=root/'objects'
source=repo/'efi/build/linux-efi-disk-20261117-block/linux-6.12'
shutil.copytree((a.base or (repo/'efi/build/linux-root-20261118/objects')).resolve(),obj,symlinks=True)
config=source/'scripts/config'
args=[str(config),'--file',str(obj/'.config'),'--set-str','LOCALVERSION','-syle-gfx1']
for key in ['VT','VT_CONSOLE','VT_HW_CONSOLE_BINDING','FB','FB_EFI','FB_SIMPLE','FB_DEVICE','FRAMEBUFFER_CONSOLE','FONT_SUPPORT','FONT_8x16','FONT_8x8']:args+=['--enable',key]
for key in ['DRM','SYSFB_SIMPLEFB','FRAMEBUFFER_CONSOLE_DEFERRED_TAKEOVER','INPUT_KEYBOARD','INPUT_MOUSE','SERIO','SERIO_I8042','KEYBOARD_ATKBD','MOUSE_PS2']:args+=['--disable',key]
subprocess.run(args,check=True)
env=dict(os.environ,ARCH='x86_64',KBUILD_BUILD_USER='syle',KBUILD_BUILD_HOST='efi',KBUILD_BUILD_TIMESTAMP='2024-11-17 00:00:00 UTC',KBUILD_BUILD_VERSION='1',SOURCE_DATE_EPOCH='1731801600',LC_ALL='C',TZ='UTC')
with (root/'build.log').open('w') as log:
 for target in [['olddefconfig'],['-j8','bzImage']]:subprocess.run(['make','-C',str(source),'O='+str(obj),'CC=gcc','HOSTCC=gcc','LD=ld']+target,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
shutil.copy2(obj/'arch/x86/boot/bzImage',root/'LINUX.EFI');shutil.copy2(obj/'.config',root/'resolved.config')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(root/'build.json').write_text(json.dumps({'base':'linux-root-20261118','kernel_sha256':sha(root/'LINUX.EFI'),'config_sha256':sha(root/'resolved.config'),'source':str(source),'initramfs':False},indent=2))
print(root)
