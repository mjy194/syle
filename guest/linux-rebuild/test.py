#!/usr/bin/env python3
"""Synthetic gate/schema tests only; these do not build or boot Linux."""
import importlib.util,json,subprocess,sys,tempfile
from pathlib import Path
sys.dont_write_bytecode=True
from recipe import FORMAT,SOURCE_SHA256,SOURCE_URL,check_config,digest,COMPRESSED_ORIGINAL_SHA256,COMPRESSED_PATCHED_SHA256,apply_compat
spec=importlib.util.spec_from_file_location('check',Path(__file__).with_name('check-inputs.py'));check=importlib.util.module_from_spec(spec);spec.loader.exec_module(check)
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);build=p/'build';build.mkdir();(build/'bzImage').write_bytes(b'test kernel')
 for f in ('config.fragment','resolved.config'):(build/f).write_bytes(Path(__file__).with_name('config.fragment').read_bytes())
 (build/'compressed-gnu11.patch').write_bytes(Path(__file__).with_name('compressed-gnu11.patch').read_bytes())
 b={'compatibility':{'path':'arch/x86/boot/compressed/Makefile','before_sha256':COMPRESSED_ORIGINAL_SHA256,'after_sha256':COMPRESSED_PATCHED_SHA256,'patch_sha256':digest(build/'compressed-gnu11.patch')},'format':FORMAT,'source_sha256':SOURCE_SHA256,'source_url':SOURCE_URL,'status':'built-not-runtime-validated','kernel_sha256':digest(build/'bzImage'),'config_sha256':digest(build/'resolved.config'),'fragment_sha256':digest(build/'config.fragment'),'tools':{k:{'path':'/synthetic/'+k,'sha256':'a'*64,'version':'host-test-only'} for k in ('gcc','cc1','make','ld')},'environment':{'ARCH':'x86_64','KBUILD_BUILD_VERSION':'1'}}
 (build/'build.json').write_text(json.dumps(b));archive=p/'archive';archive.write_bytes(b'test archive');am=p/'archive.json';am.write_text(json.dumps({'format':'syle-linux-baseline-v1','archive_sha256':digest(archive),'busybox_sha256':'a'*64,'probe_sha256':'b'*64}))
 output=p/'inputs';subprocess.run([sys.executable,str(Path(__file__).with_name('assemble-inputs.py')),'--kernel-build',str(build),'--archive',str(archive),'--archive-manifest',str(am),'--output',str(output)],check=True)
 check.verify(output,output/'manifest.json')
 for f in ('bzImage','resolved.config','config.fragment','build.json','compressed-gnu11.patch','initramfs.cpio','initramfs.manifest.json'):
  path=output/f;old=path.read_bytes();path.write_bytes(old+b'corrupt')
  try:check.verify(output,output/'manifest.json')
  except (ValueError,OSError):pass
  else:raise AssertionError('tampered input accepted: '+f)
  path.write_bytes(old)
 (build/'resolved.config').write_text('CONFIG_64BIT=n\n')
 try:check_config(build/'config.fragment',build/'resolved.config')
 except ValueError:pass
 else:raise AssertionError('unsatisfied fragment accepted')
 bad=p/'badsource';target=bad/'arch/x86/boot/compressed/Makefile';target.parent.mkdir(parents=True);target.write_bytes(b'wrong source')
 try:apply_compat(bad)
 except ValueError:pass
 else:raise AssertionError('unexpected source patched')
 assert target.read_bytes()==b'wrong source'
 r=subprocess.run([sys.executable,str(Path(__file__).with_name('build.py')),'--source-tar',str(archive),'--output',str(p/'badbuild')],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 assert r.returncode and not (p/'badbuild').exists()
print('PASS new fixture manifest/config/hash/tamper/source rejection (host synthetic only)')
