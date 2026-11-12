#!/usr/bin/env python3
"""N47A memory-only host checks. No VM, source resolver, or hardware use."""
import pathlib, subprocess, os, time
ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'efi/build/n47a-memory'
OUT.mkdir(parents=True, exist_ok=True)
for previous in OUT.glob('*.log'):
    previous.rename(previous.with_name(previous.stem + '-' + str(time.time_ns()) + '.log'))
C = r'''
#include "nested_source_replace_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static _Alignas(4096) uint64_t t[9][512];
static struct nested_root_catalog c;
static struct nrc_span spans[11];static struct nrc_root roots[3];
static struct nsrm_plan p;
static struct nrc_identity id={1,2,3,4};
static unsigned bad,reader_calls;
static int inject;
#ifdef INJECT
bool __real___atomic_compare_exchange_8(volatile void *,void *,uint64_t,int,int);
bool __wrap___atomic_compare_exchange_8(volatile void *ptr,void *expected,uint64_t desired,int success,int failure){
 if(ptr==&t[3][0]&&inject==1){__atomic_fetch_or(&t[3][0],256,__ATOMIC_RELAXED);inject=0;}
 bool ok=__real___atomic_compare_exchange_8(ptr,expected,desired,success,failure);
 if(ptr==&t[3][0]&&inject==2){__atomic_fetch_or(&t[7][1],512,__ATOMIC_RELAXED);inject=0;}
 return ok;
}
#endif
static bool old(void *ctx,const struct nrc_span *s,uint64_t g,struct nrc_atomic_page *a){(void)ctx;(void)s;(void)g;(void)a;reader_calls++;return false;}
static bool mgmt(void *ctx,uint64_t ticket,const struct nrc_span *s,uint64_t g,struct nsrm_authority_page *a){
 (void)ctx;assert(c.closed);assert(ticket==99);
 *a=(struct nsrm_authority_page){.identity=id,.ticket=ticket,.gpa=g,.object=s->object,.lifetime=s->lifetime,
  .hpa=s->hpa+g-s->gpa,.atomic_view=s->table_view?(uint64_t *)((uintptr_t)s->table_view+g-s->gpa):NULL,
  .permissions=s->permissions,.memory_type=6,.ignore_pat=true,.pinned=true,.guest_aliases_ro=s->role==NRC_TABLE,.updater_drained=true,.replacement_data_safe=g==0xb000};
 switch(bad){case 1:return false;case 2:a->identity.lifetime++;break;case 3:a->ticket++;break;
 case 4:a->gpa+=4096;break;case 5:a->object++;break;case 6:a->lifetime++;break;
 case 7:a->hpa+=4096;break;case 8:a->atomic_view=t[6];break;case 9:a->permissions^=2;break;
 case 10:a->memory_type=0;break;case 11:a->ignore_pat=false;break;case 12:a->pinned=false;break;
 case 13:a->guest_aliases_ro=false;break;case 14:a->updater_drained=false;break;case 15:a->replacement_data_safe=false;break;}
 return true;
}
static struct nsrm_request setup(void){
 memset(&c,0,sizeof c);memset(&p,0,sizeof p);memset(t,0,sizeof t);bad=0;inject=0;
 assert(nrc_init_atomic_wb(&c,id,48,spans,11,roots,3,old));
 for(unsigned i=0;i<11;i++){
  struct nrc_span s={.identity=id,.object=i+1,.lifetime=i+40,.gpa=(i+1)*4096,.hpa=(i+32)*4096,
   .bytes=4096,.table_view=i<9?t[i]:NULL,.permissions=i<9?1:7,.allowed_types=NRC_TYPE_WB,
   .role=i<9?NRC_TABLE:NRC_DATA,.ignore_pat=true};
  assert(nrc_add_atomic_span(&c,s,i<9?t[i]:NULL));
 }
 /* A and B share PDP/PT path; C is independent. */
 t[0][0]=0x2107;t[1][0]=0x3107;t[2][0]=0x4107;t[3][0]=0xa377;
 t[4][0]=0x2107;t[5][0]=0x7107;t[6][0]=0x8107;t[7][0]=0xa000|0x377;
 /* C has its own fourth-level table and unchanged control leaf. */
 t[7][0]=0x4107;
 for(unsigned i=0;i<3;i++){
  uint64_t g=i==0?0x1000:i==1?0x5000:0x6000;
  struct nrc_root r={.identity=id,.source=i+1,.owner=i,.generation=1,.lifetime=i+70,.full_eptp=g|0x5e,.root_gpa=g};
  assert(nrc_add_root(&c,r));
 }
 /* C path: t5(root)->t6(PDP)->t7(PD)->t8(PT).
  * Control root uses different slot so the target virtual address is absent. */
 t[7][0]=0;t[7][1]=0x9107;t[8][0]=0xa377;
 t[3][511]=UINT64_C(0xdead000000000000); /* ignored raw payload preserved */
 assert(nrc_publish(&c));assert(nrc_close(&c,id));
 return (struct nsrm_request){.ticket=99,.full_eptp=0x105e,.l2_gpa=0,.table_gpa=0x4000,.table_hpa=35*4096,
  .table_object=4,.table_lifetime=43,.expected_old=0xa377,.new_leaf=0xb077,.new_gpa=0xb000,.new_hpa=42*4096,.new_object=11,.new_lifetime=50,.index=0};
}
int main(void){
 struct nsrm_request r=setup();uint64_t before[9][512];memcpy(before,t,sizeof before);assert(nsrm_prepare(&p,&c,mgmt,NULL,r));assert(nsrm_apply(&p));
 for(unsigned i=0;i<9;i++)for(unsigned j=0;j<512;j++)assert(t[i][j]==(i==3&&j==0?r.new_leaf:before[i][j]));
 assert(p.started&&p.applied&&t[3][0]==0xb077&&!nsrm_apply(&p));assert(t[8][0]==0xa377);assert(t[3][511]==UINT64_C(0xdead000000000000));
 for(bad=1;bad<=15;){unsigned b=bad;r=setup();bad=b;assert(!nsrm_prepare(&p,&c,mgmt,NULL,r));assert(!p.started&&t[3][0]==0xa377);bad=b+1;}
 for(unsigned b=1;b<=15;b++){r=setup();assert(nsrm_prepare(&p,&c,mgmt,NULL,r));bad=b;assert(!nsrm_apply(&p));assert(!p.started&&t[3][0]==0xa377);}
 for(unsigned b=0;b<15;b++){
  r=setup();switch(b){case 0:r.full_eptp^=64;break;case 1:r.table_gpa+=4096;break;case 2:r.table_hpa+=4096;break;
  case 3:r.table_object++;break;case 4:r.table_lifetime++;break;case 5:r.expected_old^=256;break;
  case 6:r.new_leaf|=256;break;case 7:r.new_hpa+=4096;break;case 8:r.new_object++;break;
  case 9:r.new_lifetime++;break;case 10:r.index++;break;case 11:r.new_leaf^=64;break;
  case 12:r.l2_gpa=UINT64_C(1)<<48;break;case 13:r.new_leaf|=128;break;case 14:r.l2_gpa=1;break;}
  assert(!nsrm_prepare(&p,&c,mgmt,NULL,r));assert(!p.started);
 }
 for(unsigned b=0;b<8;b++){
  r=setup();switch(b){case 0:t[1][0]=0x1107;break;case 1:t[1][1]=0x4107;break;
  case 2:t[3][1]=0xc077;break;case 3:t[3][1]=0x1077;break;case 4:t[2][0]|=128;break;
  case 5:t[3][1]=0xa076;break;case 6:t[0][0]=0;break;case 7:t[3][1]=0xa007;break;}
  assert(!nsrm_prepare(&p,&c,mgmt,NULL,r));assert(!p.started);
 }
 r=setup();assert(nsrm_prepare(&p,&c,mgmt,NULL,r));t[7][511]=256;assert(!nsrm_apply(&p));assert(!p.started&&t[3][0]==0xa377);
 r=setup();assert(nsrm_prepare(&p,&c,mgmt,NULL,r));struct nsrm_plan copy=p;assert(!nsrm_apply(&copy));
#ifdef INJECT
 r=setup();r.expected_old=t[3][0]=0xa077;assert(nsrm_prepare(&p,&c,mgmt,NULL,r));inject=1;
 assert(!nsrm_apply(&p)&&p.started&&!p.applied&&t[3][0]==0xa177);
 r=setup();assert(nsrm_prepare(&p,&c,mgmt,NULL,r));inject=2;
 assert(!nsrm_apply(&p)&&p.started&&!p.applied&&t[3][0]==0xb077);
#endif
 assert(!reader_calls);puts("N47A memory: graph, exact authority, raw preservation and failure tests PASS");return 0;
}
'''
(OUT / 'memory.c').write_text(C)
for mode, flags in [('plain', []), ('asan-ubsan', ['-fsanitize=address,undefined','-fno-omit-frame-pointer']), ('cas-injection', ['-DINJECT','-fno-inline-atomics','-Wl,--wrap=__atomic_compare_exchange_8','-latomic']), ('tsan', ['-fsanitize=thread'])]:
    exe = OUT / mode
    cmd = ['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-I'+str(ROOT/'krnl'),str(OUT/'memory.c'),str(ROOT/'krnl/nested_source_replace_memory.c'),str(ROOT/'krnl/nested_root_catalog.c'),str(ROOT/'krnl/nested_ept.c'),'-o',str(exe)] + flags
    build = subprocess.run(cmd, text=True, capture_output=True)
    if build.returncode:
        (OUT/(mode+'.log')).write_text(build.stdout+build.stderr)
        raise SystemExit(build.stderr)
    run = subprocess.run([str(exe)],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
    (OUT/(mode+'.log')).write_text(run.stdout+run.stderr)
    if mode=='tsan' and run.returncode and 'unexpected memory mapping' in run.stderr:
        print('TSan unavailable: unexpected memory mapping (log retained)');continue
    print(mode+': '+run.stdout+run.stderr)
    if run.returncode: raise SystemExit(run.returncode)
