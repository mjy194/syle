/* Actual lgm_prepare adapter with VMCS/INVEPT host mocks; no VM entry. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main unused_memory_fixture_main
#include "linux_guest_memory.c"
#undef main
#pragma GCC diagnostic pop
#include "../linux_ept_probe.h"
struct vm {uint64_t rip,q,gpa,gla,info,vector,cr3,cs,len;unsigned writes,invalidations;bool fail_read,fail_write,fail_inv;};
static struct vm v;
static struct lgs_session *session;
static struct lep_context probe;
static struct heap *heap;
static bool rd(void*x,uint32_t f,uint64_t*out){(void)x;if(v.fail_read)return false;
 switch(f){case 0x681e:*out=v.rip;break;case 0x6400:*out=v.q;break;case 0x2400:*out=v.gpa;break;case 0x640a:*out=v.gla;break;case 0x4016:*out=v.info;break;case 0x4408:*out=v.vector;break;case 0x6802:*out=v.cr3;break;case 0x0802:*out=v.cs;break;case 0x440c:*out=v.len;break;default:assert(0);}return true;}
static bool wr(void*x,uint32_t f,uint64_t value){(void)x;v.writes++;if(v.fail_write)return false;assert(f==0x681e);v.rip=value;return true;}
static bool inv(void*x,uint64_t eptp){(void)x;assert(eptp==session->memory.eptp);v.invalidations++;return !v.fail_inv;}
static void setup(void){
 session=calloc(1,sizeof*session);heap=calloc(1,sizeof*heap);assert(session&&heap);
 assert(!posix_memalign((void**)&heap->bytes,4096,(size_t)COUNT*4096));
 unsigned char image[8192]={0},initrd[131]={1,2,3};
 image[0x200]=0xeb;image[0x201]=0x6a;image[0x1f1]=4;put(image+0x1f4,4,(sizeof image-2560)/16);put(image+0x1fe,2,0xaa55);put(image+0x202,4,0x53726448);put(image+0x206,2,0x20f);image[0x211]=1;put(image+0x22c,4,0x7fffffff);put(image+0x230,4,0x200000);image[0x234]=1;put(image+0x236,2,1);put(image+0x238,4,2048);put(image+0x258,8,0x1000000);put(image+0x260,4,0x100000);
 struct sg_memory_ops ops={heap,alloc,release,map,wb};
 assert(lgm_prepare(&session->memory,&ops,image,sizeof image,initrd,sizeof initrd,"console=ttyS0 rdinit=/init"));
 session->platform.read_field=rd;session->hop.vmwrite=wr;session->hop.invept=inv;
 v=(struct vm){.cr3=0x1000,.cs=0x10,.len=3};
 struct lgm_service_ops service;assert(lep_init(&probe,&service));assert(service.prepare_guest==lep_prepare&&service.dispatch==lep_dispatch);
}
static void finish(void){assert(lgm_release(&session->memory)&&heap->live==0);free(heap->bytes);free(heap);free(session);}
static void ready(void){setup();assert(lep_prepare(&probe,session));assert(((*probe.data_leaf)&7)==0);assert(lep_before(&probe,session));assert(v.rip==LEP_CODE&&v.writes==1&&v.invalidations==1);}
static void fault(unsigned phase){
 v.rip=phase==0?probe.read_rip:phase==1?probe.write_rip:LEP_EXEC;
 v.gla=v.gpa=phase<2?LEP_DATA:LEP_EXEC;
 unsigned access=phase==0?1:phase==1?2:4,permissions=phase==0?0:phase==1?5:3;
 v.q=0x180|access|(permissions<<3);
}
static void checkpoint(unsigned phase){
 v.rip=phase==0?probe.read_done:phase==1?probe.write_done:probe.exec_done;
 session->native.gpr[0]=phase+1;session->native.gpr[1]=phase==0?LEP_INITIAL:phase==1?LEP_WRITTEN:LEP_EXEC_VALUE;
 if(phase==1)*(uint64_t*)map(heap,session->memory.pages[LEP_DATA/4096])=LEP_WRITTEN;
}
int main(void){
 ready();
 for(unsigned phase=0;phase<3;phase++){
  fault(phase);uint64_t oldrip=v.rip;unsigned oldwrites=v.writes;
  assert(lep_dispatch(&probe,session,48));assert(v.rip==oldrip&&v.writes==oldwrites&&probe.repaired&&probe.faults==phase+1);
  checkpoint(phase);assert(lep_dispatch(&probe,session,18));
 }
 assert(probe.complete&&probe.faults==3);assert(*probe.data_leaf==probe.data_original&&*probe.exec_leaf==probe.exec_original);
 assert(lep_recv(&probe)=='P'&&lep_recv(&probe)==-SYLE_GUEST_EBUSY);assert(!lep_before(&probe,session));finish();
 for(unsigned mode=0;mode<10;mode++){
  ready();fault(0);
  if(mode==0)v.rip++;
  if(mode==1)v.gpa++;
  if(mode==2)v.gla++;
  if(mode==3)v.q=0x182;
  if(mode==4)v.vector=0x8000000e;
  if(mode==5)v.info=0x80000b0e;
  if(mode==6)v.cs=0x13;
  if(mode==7)v.fail_read=true;
  if(mode==8)v.fail_inv=true;
  if(mode==9)*probe.data_leaf=probe.data_original;
  assert(!lep_dispatch(&probe,session,48)&&probe.failed&&!probe.complete);
  assert(!lep_before(&probe,session));assert(lep_recv(&probe)<0);finish();
 }
 ready();fault(0);assert(lep_dispatch(&probe,session,48));assert(!lep_dispatch(&probe,session,48));finish();
 for(unsigned mode=0;mode<4;mode++){
  ready();fault(0);assert(lep_dispatch(&probe,session,48));checkpoint(0);
  if(mode==0)session->native.gpr[1]++;
  if(mode==1)v.len=4;
  if(mode==2)v.fail_write=true;
  if(mode==3)v.fail_inv=true;
  assert(!lep_dispatch(&probe,session,18)&&probe.failed);finish();
 }
 setup();session->memory.plan.initrd_addr=LEP_CODE;assert(!lep_prepare(&probe,session)&&probe.failed);finish();
 setup();((uint64_t*)map(heap,session->memory.pages[3]))[0]=0;assert(!lep_prepare(&probe,session));finish();
 ready();struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=48};assert(lep_after(&probe,session,&r));r.vectoring=0x80000b0e;assert(!lep_after(&probe,session,&r));finish();
 puts("PASS managed EPT probe host contracts: actual private RAM setup, R/W/X retry with unchanged RIP, strict checkpoints, sticky failures; no hardware");return 0;
}
