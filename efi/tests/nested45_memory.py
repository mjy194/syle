#!/usr/bin/env python3
"""Fresh-source constructor and production-owner gates. No VMX claim."""
import argparse,os,pathlib,shutil,subprocess,tempfile,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--sanitizer',choices=['none','address,undefined','thread'],default='none');a=p.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];out=pathlib.Path(tempfile.mkdtemp(prefix='syle-n45-memory-'));print('N45_MEMORY_EVIDENCE',out,flush=True)
for d in ('krnl','include','efi/tests','efi/src'):shutil.copytree(repo/d,out/d)
legacy=(out/'efi/tests/nested41c_memory.c').read_text().replace('ram[NAM_OWNED_PAGES]', 'ram[NAM_OWNED_PAGES+10]').replace('(NAM_OWNED_PAGES+1)*4096', '(NAM_OWNED_PAGES+11)*4096').replace('allocated<NAM_OWNED_PAGES', 'allocated<NAM_OWNED_PAGES+10').replace('before[NAM_OWNED_PAGES]', 'before[NAM_OWNED_PAGES+10]').replace('saved[NAM_OWNED_PAGES]', 'saved[NAM_OWNED_PAGES+10]')
(out/'efi/tests/nested41c_memory.c').write_text(legacy)
fixture=(out/'krnl/nested_owner_atomic_maintenance_test.c').read_text().replace('int main(void)','int unused_owner_main(void)')
assert fixture.count('assert(nested_atomic_memory_prepare(&m,&d,id));')==1
fixture=fixture.replace('assert(nested_atomic_memory_prepare(&m,&d,id));','assert(nested_atomic_memory_prepare_1g(&m,&d,id));')
(out/'krnl/n45_owner_fixture.inc').write_text(fixture)
round_fixture=fixture.replace('0xf000','0x4000f000').replace('0xf008','0x4000f008').replace('page_count==9','page_count==11').replace('ram[18][15]','ram[16][1]')
round_fixture=round_fixture.replace('a->sparse=s;a->vcpu=', 'a->target_gpa=0x4000f000;a->sparse=s;a->vcpu=')
(out/'krnl/n45_round_fixture.inc').write_text(round_fixture)
rounds=(out/'efi/tests/nested43_test.c').read_text().split('static void snapshot_first')[0].replace('n43_fixture_base.inc','n45_round_fixture.inc').replace('0xf000','0x4000f000').replace('0xf008','0x4000f008').replace('ram[18][15]','ram[16][1]')
rounds+=(out/'efi/tests/nested45_rounds_main.c').read_text()
(out/'krnl/n45_rounds.c').write_text(rounds)
shutil.copy2(out/'efi/tests/nested45_owner_test.c',out/'krnl/n45_owner_test.c')
inc=os.environ.get('EFI_PREFIX','/usr')+'/include/efi'
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-I'+inc,'-I'+inc+'/x86_64','-I'+str(out/'krnl')]
flags+=['-D'+x for x in ('VMX_NESTED_DOMAIN_TEST','VMX_NESTED_OWNER_SPARSE_TEST','VMX_NESTED_OWNER_CONTEXT_TEST','VMX_NESTED_ATOMIC_CATALOG_TEST')]
if a.sanitizer!='none':flags+=['-fsanitize='+a.sanitizer]
common='nested_atomic_memory.c nested_domain_memory.c nested_root_catalog.c nested_ept.c nested_ept_atomic.c ad_updater_gate.c'.split()
owner='nested_atomic_retrack.c nested_owner_atomic_maintenance.c nested_owner_atomic.c nested_vcpu_policy.c nested_domain_dispatch.c nested_domain.c nested_vmx.c typed_root_atomic.c typed_root_registry.c typed_root_lease.c typed_root_local_provider.c typed_directory_registry.c typed_directory.c nested_atomic_catalog.c nested_owner_pool.c nested_ept_contexts.c nested_ept_sparse.c'.split()
(out/'source-sha256.json').write_text(json.dumps({str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for d in ('krnl','efi/tests') for f in (out/d).glob('*') if f.is_file()},indent=2))
for name,first,extra in [('memory',out/'efi/tests/nested45_memory.c',[]),('owner',out/'krnl/n45_owner_test.c',owner),('rounds',out/'krnl/n45_rounds.c',owner+['nested_atomic_multiround.c'])]:
 cmd=[os.environ.get('CC','cc'),*flags,str(first),*[str(out/'krnl'/s) for s in common+extra],'-o',str(out/name)]
 result=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/(name+'-build.log')).write_text(result.stdout);assert result.returncode==0,result.stdout
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0';env['TSAN_OPTIONS']='halt_on_error=1'
 result=subprocess.run([str(out/name)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,timeout=90);(out/(name+'-run.log')).write_text(result.stdout);print(result.stdout,end='');assert result.returncode==0,result.returncode
print('PASS N45 fresh memory/owner gates; sanitizer='+a.sanitizer+'; no hardware claim')
