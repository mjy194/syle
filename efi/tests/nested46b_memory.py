#!/usr/bin/env python3
"""Production unpublished fixed-UC fixture, physical primitives mocked."""
import argparse,os,pathlib,shutil,subprocess,tempfile,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='none');a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n46b-memory-'));print('N46B_MEMORY_EVIDENCE',out,flush=True)
for d in ('krnl','include','efi/tests','efi/src'):shutil.copytree(repo/d,out/d)
inc=os.environ.get('EFI_PREFIX','/usr')+'/include/efi'
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-I'+inc,'-I'+inc+'/x86_64','-I'+str(out/'krnl')]
flags+=['-D'+x for x in ('VMX_NESTED_DOMAIN_TEST','VMX_NESTED_OWNER_SPARSE_TEST','VMX_NESTED_OWNER_CONTEXT_TEST','VMX_NESTED_ATOMIC_CATALOG_TEST')]
if a.sanitizer!='none':flags+=['-fsanitize='+a.sanitizer]
sources=['nested_atomic_memory.c','nested_domain_memory.c','nested_root_catalog.c','nested_ept.c']
(out/'source-sha256.json').write_text(json.dumps({str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for d in ('krnl','efi/tests') for f in (out/d).glob('*') if f.is_file()},sort_keys=True,indent=2)+'\n')
result=subprocess.run([os.environ.get('CC','cc'),*flags,str(out/'efi/tests/nested46b_memory.c'),*[str(out/'krnl'/s) for s in sources],'-o',str(out/'test')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/'build.log').write_text(result.stdout);assert result.returncode==0,result.stdout
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',TSAN_OPTIONS='halt_on_error=1')
result=subprocess.run([str(out/'test')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,timeout=90);(out/'run.log').write_text(result.stdout);print(result.stdout,end='');assert result.returncode==0,result.returncode
assert sum(s.startswith('PASS N46B') for s in result.stdout.splitlines())==20
print('PASS N46B actual staged fixture: 20 cases; sanitizer='+a.sanitizer+'; no physical UC claim')
