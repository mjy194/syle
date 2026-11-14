#!/usr/bin/env python3
"""Combine explicit new kernel build with separately hashed initramfs fixture."""
import argparse,json,shutil
from pathlib import Path
from recipe import load_build,digest,FORMAT,check_archive_manifest
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--kernel-build',type=Path,required=True);p.add_argument('--archive',type=Path,required=True);p.add_argument('--archive-manifest',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
b=load_build(a.kernel_build);m=json.loads(a.archive_manifest.read_text());check_archive_manifest(m)
if m.get('format')!='syle-linux-baseline-v1' or digest(a.archive)!=m.get('archive_sha256'):raise ValueError('initramfs manifest mismatch')
a.output.mkdir(parents=False,exist_ok=False)
for name in ('bzImage','resolved.config','config.fragment','build.json','compressed-gnu11.patch'):shutil.copyfile(a.kernel_build/name,a.output/name)
shutil.copyfile(a.archive,a.output/'initramfs.cpio');shutil.copyfile(a.archive_manifest,a.output/'initramfs.manifest.json')
manifest={'format':FORMAT,'kernel_sha256':b['kernel_sha256'],'archive_sha256':m['archive_sha256'],'config_sha256':b['config_sha256'],'build_manifest_sha256':digest(a.output/'build.json'),'archive_manifest_sha256':digest(a.output/'initramfs.manifest.json')}
(a.output/'manifest.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
