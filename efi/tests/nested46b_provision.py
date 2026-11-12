#!/usr/bin/env python3
"""Fresh-copy N46B bootstrap controller tests; no physical primitive claims."""
import argparse, hashlib, json, os, pathlib, resource, shutil, subprocess, tempfile
p=argparse.ArgumentParser()
p.add_argument('--sanitizer', choices=['none','address,undefined','thread'], default='address,undefined')
a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2]
out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n46b-provision-'))
print('N46B_PROVISION_EVIDENCE', out, flush=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
files=['krnl/nested_atomic_uc_provision.h','krnl/nested_atomic_uc_provision.c','efi/tests/nested46b_provision_test.c']
for name in files: shutil.copy2(repo/name,out/pathlib.Path(name).name)
(out/'source-sha256.json').write_text(json.dumps({name:hashlib.sha256((repo/name).read_bytes()).hexdigest() for name in files},indent=2)+'\n')
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie','-I'+str(out)]
if a.sanitizer!='none': flags+=['-fsanitize='+a.sanitizer]
cmd=[os.environ.get('CC','cc'),*flags,str(out/'nested_atomic_uc_provision.c'),str(out/'nested46b_provision_test.c'),'-o',str(out/'test')]
r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
(out/'build.log').write_text(r.stdout)
assert r.returncode==0,r.stdout
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0';env['TSAN_OPTIONS']='halt_on_error=1'
r=subprocess.run([str(out/'test')],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env,timeout=90)
(out/'run.log').write_text(r.stdout);print(r.stdout,end='');assert r.returncode==0,r.returncode
assert '1217 cases, 40 exact actions' in r.stdout
(out/'results.json').write_text(json.dumps({'status':'PASS','cases':1217,'actions':40,'sanitizer':a.sanitizer,'hardware_claim':False},indent=2)+'\n')
