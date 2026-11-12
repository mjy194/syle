#!/usr/bin/env python3
"""Focused production N47A nonce successors, fresh source snapshots; no VMX."""
import argparse, hashlib, json, os, pathlib, resource, shutil, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='address,undefined');a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2]
out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n47a-successor-'));print('N47A_SUCCESSOR_EVIDENCE',out,flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));shutil.copytree(repo/'krnl',out/'krnl')
sources='nested47a_successor_test.c nested_root_catalog.c ad_updater_gate.c nested_atomic_catalog.c nested_ept_atomic.c nested_ept.c'.split()
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie']
if a.sanitizer!='none':flags+=['-fsanitize='+a.sanitizer]
(out/'source-sha256.json').write_text(json.dumps({f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (out/'krnl').glob('*') if f.is_file()},indent=2)+'\n')
with (out/'build.log').open('w') as f:r=subprocess.run([os.environ.get('CC','cc'),*flags,*[str(out/'krnl'/s) for s in sources],'-o',str(out/'test')],stdout=f,stderr=subprocess.STDOUT)
assert r.returncode==0,(out/'build.log').read_text()
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TSAN_OPTIONS='halt_on_error=1')
with (out/'run.log').open('w') as f:r=subprocess.run([str(out/'test')],stdout=f,stderr=subprocess.STDOUT,env=env,timeout=90)
log=(out/'run.log').read_text();print(log,end='')
(out/'results.json').write_text(json.dumps({'sanitizer':a.sanitizer,'exit_code':r.returncode,'status':'PASS' if r.returncode==0 else 'FAIL','hardware_claim':False},indent=2)+'\n')
assert r.returncode==0,('runtime failed; logs retained',out,r.returncode)
assert log.count('PASS N47A ')==5
