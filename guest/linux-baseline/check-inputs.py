#!/usr/bin/env python3
"""Validate the explicit baseline fixture; historical build rules are unchanged."""
import argparse, hashlib, json, pathlib, re
KERNEL_SHA256='3d1c081c18e64ad97a44e2b038b8b3d0c733d38494b6293ba2402898c3a629e6'
def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
    return h.hexdigest()
def verify(inputs,manifest):
    data=json.loads(manifest.read_text())
    if data.get('format')!='syle-linux-baseline-v1': raise ValueError('unsupported baseline manifest format')
    for key in ['archive_sha256','busybox_sha256','probe_sha256']:
        if not isinstance(data.get(key),str) or not re.fullmatch('[0-9a-f]{64}',data[key]):
            raise ValueError('missing or invalid '+key)
    if digest(inputs/'bzImage')!=KERNEL_SHA256: raise ValueError('kernel differs from frozen Linux baseline')
    if digest(inputs/'initramfs.cpio')!=data['archive_sha256']: raise ValueError('initramfs differs from explicit baseline manifest')
    return data
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--inputs',type=pathlib.Path,required=True)
    p.add_argument('--manifest',type=pathlib.Path,required=True)
    a=p.parse_args(); verify(a.inputs,a.manifest)
    print('PASS explicit baseline kernel/initramfs hash gate')
