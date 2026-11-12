#!/usr/bin/env python3
"""Independent N40B unchanged-production adapter/gate tests; no kernel build."""
from pathlib import Path
import os, shlex, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[2]
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
recipe=".PHONY: review-flags\nreview-flags:\n\t@printf '%s\\n' '$(N40BFLAGS)'\n"
DEFINES=shlex.split(subprocess.check_output(['make','--no-print-directory','-s','-f','Makefile','-f','-','review-flags'],input=recipe,text=True,cwd=ROOT/'efi'))
FLAGS=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',*DEFINES,'-I'+str(ROOT/'krnl')]
source=(ROOT/'krnl/nested_typed_dynamic_maintenance.inc').read_text()
def function(name):
 seam='static bool '+name+'('
 assert source.count(seam)==1
 a=source.index(seam);b=source.index('{',a);depth=1;e=b+1
 while depth:
  depth+=(source[e]=='{')-(source[e]=='}');e+=1
 return source[a:e]+'\n'
def result(binary,mutant=False):
 r=subprocess.run([str(binary)],env=ENV,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
 if mutant:
  assert r.returncode==-6 and 'Assertion' in r.stdout,r.stdout
  print('PASS independent N40B mutant rejected: '+binary.name)
 else:
  assert r.returncode==0,r.stdout
  print(r.stdout,end='')
def compile_run(out,name,text,extra=(),mutant=False):
 path=out/(name+'.c');path.write_text(text);binary=out/name
 subprocess.run(['cc',*FLAGS,str(path),*[str(ROOT/'krnl'/f) for f in extra],'-o',str(binary)],check=True)
 result(binary,mutant)
entry_prefix=r'''
#include "nested_parallel.h"
#include "nested_owner_dynamic_typed.h"
#include "cache_mp_transition.h"
#include <assert.h>
#include <stdio.h>
static struct {
 struct typed_root_registry bridges[2];struct typed_directory directory;
 struct typed_root_lease lease;struct trl_binding fixed_bindings[5];
 struct nested_owner_dynamic_typed owners[2];struct cache_mp_transition transition;
 bool initialized,source_registered;uint64_t source_epoch;
} typed_runtime;
static unsigned env_calls,catalog_calls,lock_calls,fatal_calls,depth;
static bool env_ok,catalog_ok;
static bool typed_failed(void){return false;}
static void typed_fatal(void *p){(void)p;fatal_calls++;}
static bool typed_overlap_entry(struct nested_parallel_runtime *r){(void)r;return true;}
static bool typed_guard(void *r){(void)r;assert(!depth);env_calls++;return env_ok;}
bool nested_owner_catalog_entry(struct nested_owner_dynamic_typed *t){(void)t;assert(!depth);catalog_calls++;return catalog_ok;}
static bool typed_meta_lock(void){lock_calls++;return false;}
static void typed_meta_unlock(void){assert(depth);depth=0;}
'''
entry_main=r'''
static void run(bool env,bool catalog,bool l2,unsigned expected_catalog,unsigned expected_lock){
 env_calls=catalog_calls=lock_calls=fatal_calls=depth=0;env_ok=env;catalog_ok=catalog;
 struct nested_parallel_runtime r={0};typed_runtime.initialized=true;
 assert(!typed_context_before_entry(&r,l2));
 assert(env_calls==1&&catalog_calls==expected_catalog&&lock_calls==expected_lock&&fatal_calls==1&&!depth);
}
int main(void){run(true,true,true,1,1);run(true,false,true,1,0);run(false,true,true,0,0);run(true,true,false,0,1);puts("PASS independent unchanged runtime before-entry validates catalog/environment before metadata lock");}
'''
retype_prefix=r'''
#include "nested_owner_dynamic_typed.h"
#include <assert.h>
#include <stdio.h>
static struct {bool source_registered;struct nested_owner_dynamic_typed owners[2];} typed_runtime;
static struct nested_root_catalog typed_catalog;
static uint64_t pages[3][512],*typed_catalog_type_leaves[3];
static bool idle;
static size_t typed_source_count(void){return 3;}
bool nrc_idle(struct nested_root_catalog *c,struct nrc_identity i){(void)c;(void)i;return idle;}
'''
retype_main=r'''
static void setup(void){idle=true;typed_runtime.source_registered=false;for(unsigned i=0;i<2;i++)typed_runtime.owners[i].source_session.live=false;for(unsigned i=0;i<3;i++){pages[i][15]=0xf073;typed_catalog_type_leaves[i]=pages[i]+15;}}
static void unchanged(void){for(unsigned i=0;i<3;i++)assert(pages[i][15]==0xf073);}
int main(void){
 setup();idle=false;assert(!typed_source_retype(6,0));unchanged();
 for(unsigned i=0;i<2;i++){setup();typed_runtime.owners[i].source_session.live=true;assert(!typed_source_retype(6,0));unchanged();}
 setup();typed_runtime.source_registered=true;assert(!typed_source_retype(6,0));unchanged();
 setup();pages[2][15]=0;assert(!typed_source_retype(6,0));assert(pages[0][15]==0xf073&&pages[1][15]==0xf073);
 setup();assert(typed_source_retype(6,0));for(unsigned i=0;i<3;i++)assert(pages[i][15]==0xf043);
 puts("PASS independent unchanged source-retype gate rejects live sessions, registered sources and nonidle catalog before writes");
}
'''
release_prefix=r"""
#include "nested_owner_dynamic_typed.h"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
static struct {struct nested_owner_dynamic_typed owners[2];} typed_runtime;
static struct nested_root_catalog typed_catalog;
static int domain_memory;
static bool close_ok,drain_ok;static unsigned closes,drains,frees;static jmp_buf escape;
bool nrc_close(struct nested_root_catalog *c,struct nrc_identity i){(void)c;(void)i;closes++;return close_ok;}
bool nrc_drained(struct nested_root_catalog *c,struct nrc_identity i){(void)c;(void)i;drains++;return drain_ok;}
static _Noreturn void panic(const char *p){(void)p;longjmp(escape,1);}
static void log_text(const char *p){(void)p;}
static void log_hex(uint64_t n){(void)n;}
static void nested_domain_memory_release(int *m){(void)m;frees++;}
"""
release_main=r"""
static void run(unsigned failure){
 closes=drains=frees=0;close_ok=drain_ok=true;
 for(unsigned i=0;i<2;i++){typed_runtime.owners[i].catalog_checks=15;typed_runtime.owners[i].source_session.live=false;}
 if(failure==1)typed_runtime.owners[0].source_session.live=true;
 if(failure==2)typed_runtime.owners[1].catalog_checks=7;
 if(failure==3)close_ok=false;
 if(failure==4)drain_ok=false;
 int panicked=setjmp(escape);if(!panicked)release_backing();
 assert((panicked!=0)==(failure!=0));assert(frees==(failure?0u:1u));
 if(!failure)assert(closes==1&&drains==1);
 if(failure<=2&&failure)assert(!closes&&!drains);
 if(failure==3)assert(closes==1&&!drains);
 if(failure==4)assert(closes==1&&drains==1);
}
int main(void){for(unsigned f=0;f<=4;f++)run(f);puts("PASS independent unchanged final release requires negative-probe completion, no sessions, closed and drained catalog");}
"""
with tempfile.TemporaryDirectory(prefix='syle-n40b-independent-') as temp:
 out=Path(temp)
 # Production owner adapter, real registry/pool, existing mocked physical hooks.
 # Alternate fixture drops root L1 GPA from L2 before publication. Generic bind
 # has no diagnostic negative probe and must still support valid data accesses.
 fixture=(ROOT/'krnl/tests/nested_owner_catalog.c').read_text()
 assert fixture.count('int main(void)')==1 and fixture.count('assert(nrc_publish(&catalog));')==1
 fixture=fixture.replace('int main(void)','int original_suite(void)')
 fixture=fixture.replace('assert(nrc_publish(&catalog));','if(review_no_selfmap){fixture_memory[18][16]=0;}assert(nrc_publish(&catalog));')
 fixture='static int review_no_selfmap;\n'+fixture
 fixture=fixture.replace('#include "../','#include "'+str(ROOT/'krnl')+'/')
 fixture+=r'''
static bool generic_ready(struct nested_owner_sparse *s){return s==&owner&&state.bound;}
int main(void){original_suite();review_no_selfmap=1;configure();owner.contexts_ready=generic_ready;
 assert(nested_owner_sparse_bootstrap_contexts(&owner,&ops,&owner));
 assert(nested_owner_sparse_miss(&owner,0xf000,3)&&nested_owner_catalog_entry(&state));
 assert(state.catalog_checks==0);unsigned allocation=test.allocations;
 assert(!nested_owner_sparse_select(&owner,0x1005e)&&test.allocations==allocation);
 assert(nested_owner_catalog_entry(&state));assert(nrc_close(&catalog,catalog.identity));
 assert(!nested_owner_catalog_entry(&state)&&!nested_owner_sparse_select(&owner,0x1001e));
 assert(test.allocations==allocation);stop();
 puts("PASS independent generic root L1 GPA need not be mapped in L2; warm entry rejects closed catalog and wrong full config");return 0;}
'''
 files=['nested_owner_dynamic_typed.c','nested_owner_sparse.c','nested_root_catalog.c','typed_root_registry.c','typed_root_lease.c','typed_directory_registry.c','typed_directory.c','nested_ept.c','nested_ept_contexts.c','nested_ept_sparse.c','nested_owner_pool.c','typed_root_overlap.c']
 compile_run(out,'adapter',fixture,files)
 adapter=(ROOT/'krnl/nested_owner_catalog.inc').read_text()
 adapter_mutants=[
  ('restored_selfmap_requirement','t->bound=true;return true;','t->bound=true;return nested_owner_catalog_probe(t,t->selected_source.root_gpa,0x40000)||fail(t);'),
  ('missing_catalog_binding','t->catalog==t->bound_catalog&&',''),
  ('missing_selected_generation','a.generation==b.generation&&',''),
  ('missing_actual_ept01_mapping','pt[gpa>>12]!=(hpa|0x40|((uint64_t)type<<3)|span->permissions)','(pt[gpa>>12]!=(hpa|0x40|((uint64_t)type<<3)|span->permissions)&&false)'),
 ]
 for name,old,new in adapter_mutants:
  assert adapter.count(old)==1,(name,adapter.count(old))
  mutation=out/(name+'_adapter.c');mutation.write_text(adapter.replace(old,new))
  compile_run(out,name,fixture,[str(mutation),*files[1:]],mutant=True)
 entry=function('typed_context_before_entry')
 compile_run(out,'entry',entry_prefix+entry+entry_main)
 guard='       (in_l2&&!nested_owner_catalog_entry(&typed_runtime.owners[cpu]))||\n'
 assert entry.count(guard)==1
 # Restore the old integration error: validate source only after acquiring lock.
 moved=entry.replace(guard,'').replace('    struct trl_binding *binding=', '    if(in_l2&&!nested_owner_catalog_entry(&typed_runtime.owners[cpu])){typed_meta_unlock();return false;}\n    struct trl_binding *binding=')
 compile_run(out,'guard_after_lock',entry_prefix+moved+entry_main,mutant=True)
 compile_run(out,'missing_entry_guard',entry_prefix+entry.replace(guard,'')+entry_main,mutant=True)
 retype=function('typed_source_retype')
 compile_run(out,'retype',retype_prefix+retype+retype_main)
 for name,old,new in [('missing_idle','!nrc_idle(&typed_catalog,typed_catalog.identity)','(!nrc_idle(&typed_catalog,typed_catalog.identity)&&false)'),('missing_owner0_session','typed_runtime.owners[0].source_session.live||',''),('missing_owner1_session','typed_runtime.owners[1].source_session.live||','')]:
  assert retype.count(old)==1
  compile_run(out,name,retype_prefix+retype.replace(old,new)+retype_main,mutant=True)
 final_source=(ROOT/'krnl/smp_nested.inc').read_text()
 # Keep complete production conditionals around the shared backing release.
 # Ending at the release statement can truncate a later profile's #else.
 start="#ifdef VMX_NESTED_ROOT_CATALOG_TEST\n    for(unsigned cpu=0;cpu<2;cpu++){"
 end='#ifdef VMX_NESTED_SOURCE_REPLACE_TEST\n    log_text("N47B_ALL_BACKING_RETAINED=1\\n");'
 assert final_source.count(start)==1 and final_source.count(end)==1
 release=final_source[final_source.index(start):final_source.index(end)]
 assert release.count('    nested_domain_memory_release(&domain_memory);')==1
 release='static void release_backing(void){\n'+release+'\n}\n'
 compile_run(out,'release',release_prefix+release+release_main)
 old='!nrc_drained(&typed_catalog,typed_catalog.identity)'
 assert release.count(old)==1
 mutant=release.replace(old,'(!nrc_drained(&typed_catalog,typed_catalog.identity)&&false)')
 compile_run(out,'missing_final_drain',release_prefix+mutant+release_main,mutant=True)
print('PASS N40B independent adapter and unchanged production gates, ten mutation witnesses; physical operations mocked')
