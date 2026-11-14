#!/usr/bin/env python3
"""Explicit NEW kernel/recipe/config/initramfs gate; historical pin unchanged."""
import argparse,json
from pathlib import Path
from recipe import load_build,digest,FORMAT,check_archive_manifest

def verify(inputs,manifest):
 b=load_build(inputs);m=json.loads(Path(manifest).read_text())
 if m.get('format')!=FORMAT:raise ValueError('wrong new-fixture format')
 for name,key in [('bzImage','kernel_sha256'),('resolved.config','config_sha256'),('initramfs.cpio','archive_sha256'),('build.json','build_manifest_sha256'),('initramfs.manifest.json','archive_manifest_sha256')]:
  if digest(inputs/name)!=m.get(key):raise ValueError('input mismatch: '+name)
 archive=json.loads((inputs/'initramfs.manifest.json').read_text());check_archive_manifest(archive)
 if archive.get('format')!='syle-linux-baseline-v1' or archive.get('archive_sha256')!=m['archive_sha256']:raise ValueError('archive provenance mismatch')
 return b
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--inputs',type=Path,required=True);p.add_argument('--manifest',type=Path,required=True);a=p.parse_args();verify(a.inputs,a.manifest);print('PASS new source/config/kernel/initramfs recipe gate; no runtime claim')
