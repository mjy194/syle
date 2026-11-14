#!/usr/bin/env python3
"""Unit tests use a synthetic kernel pin in memory; production pin is unchanged."""
import importlib.util,json,pathlib,tempfile,hashlib,sys
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('check',pathlib.Path(__file__).with_name('check-inputs.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
assert m.KERNEL_SHA256=='3d1c081c18e64ad97a44e2b038b8b3d0c733d38494b6293ba2402898c3a629e6'
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d); (p/'bzImage').write_bytes(b'kernel');(p/'initramfs.cpio').write_bytes(b'archive')
 m.KERNEL_SHA256=hashlib.sha256(b'kernel').hexdigest()
 j={'format':'syle-linux-baseline-v1','archive_sha256':hashlib.sha256(b'archive').hexdigest(),'busybox_sha256':'a'*64,'probe_sha256':'b'*64}
 manifest=p/'manifest.json';manifest.write_text(json.dumps(j));m.verify(p,manifest)
 def rejects():
  try:m.verify(p,manifest)
  except (ValueError,OSError):return
  raise AssertionError('invalid input accepted')
 (p/'bzImage').write_bytes(b'wrong kernel');rejects();(p/'bzImage').write_bytes(b'kernel')
 (p/'initramfs.cpio').write_bytes(b'changed');rejects();(p/'initramfs.cpio').write_bytes(b'archive')
 for field in ('format','archive_sha256','busybox_sha256','probe_sha256'):
  bad=dict(j);bad[field]='invalid';manifest.write_text(json.dumps(bad));rejects()
 manifest.unlink();rejects()
print('PASS explicit input gate schema/kernel/archive mismatch rejection (host only)')
