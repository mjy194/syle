#!/usr/bin/env python3
"""Exact transformation test using a Makefile from the pinned source archive."""
import argparse,subprocess,tempfile
from pathlib import Path
from recipe import apply_compat,digest,COMPRESSED_ORIGINAL_SHA256,COMPRESSED_PATCHED_SHA256
p=argparse.ArgumentParser();p.add_argument('--original',type=Path,required=True);a=p.parse_args()
assert digest(a.original)==COMPRESSED_ORIGINAL_SHA256
with tempfile.TemporaryDirectory() as d:
 root=Path(d);target=root/'arch/x86/boot/compressed/Makefile'
 target.parent.mkdir(parents=True);target.write_bytes(a.original.read_bytes())
 record=apply_compat(root);assert record['after_sha256']==COMPRESSED_PATCHED_SHA256
 first=target.read_bytes()
 try:apply_compat(root)
 except ValueError:pass
 else:raise AssertionError('double application accepted')
 assert target.read_bytes()==first
 subprocess.run(['patch','--reverse','--batch','-p1','-i',str(Path(__file__).with_name('compressed-gnu11.patch').resolve())],cwd=root,check=True)
 assert digest(target)==COMPRESSED_ORIGINAL_SHA256
print('PASS pinned-source patch transformation, reapplication rejection and reversal')
