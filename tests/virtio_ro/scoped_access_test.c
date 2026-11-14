#include "scoped_access.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
static struct {uint32_t type,pad;uint64_t base,virt,pages,attr;} desc={7,0,0x100000,0,2,8};
static unsigned char backing[2][4096],external[4096];
static struct vsc s;
static unsigned transfers;
static bool close_inside,partial_fail,close_in_buffer;
static struct vlm_allocation active_page;
static uint64_t ordinary[PMM_MAX_RANGES*2+1];
static jmp_buf fatal_jump;
static unsigned fatal_phase;
static bool host_buffer(void *ctx,const void *p,size_t n){
 assert(ctx==&s&&s.busy&&s.borrowers==1);
 if(fatal_phase==1)longjmp(fatal_jump,1);
 struct vlm_allocation out={0};
 assert(!vsc_copy(&s,active_page,VLM_GUEST_RAM,0,external,1,true));
 assert(!vsc_allocate(&s,SG_MEMORY_RAM,&out)&&!out.generation);
 assert(!vsc_retire_after_quiescence(&s));
 if(close_in_buffer)assert(!vsc_drain(&s));
 return p==external&&n<=sizeof external;
}
static bool transfer(void *ctx,uint64_t pa,size_t offset,void *p,size_t n,bool write){
 assert(ctx==&s&&s.borrowers==1&&s.busy);
 if(fatal_phase==2)longjmp(fatal_jump,1);
 transfers++;assert(pa==0x100000||pa==0x101000);
 if(close_inside){assert(!vsc_drain(&s));assert(!vsc_retire_after_quiescence(&s));assert(pmm_get_stats().allocated_pages==1);}
 unsigned char *owned=backing[(pa-0x100000)/4096]+offset;
 if(partial_fail){if(write)owned[0]=*(unsigned char*)p;else *(unsigned char*)p=owned[0];return false;}
 if(write)memcpy(owned,p,n);else memcpy(p,owned,n);return true;
}
int main(int argc,char **argv){assert(argc==1||argc==2);bool fragment=argc==2&&!strcmp(argv[1],"fragment");if(fragment)desc.pages=PMM_MAX_RANGES*2+2;struct syle_boot_info bi={0};bi.magic=SYLE_BOOT_MAGIC;bi.version=SYLE_BOOT_VERSION;bi.size=sizeof(bi);bi.boot_services_exited=1;bi.descriptor_size=sizeof(desc);bi.descriptor_version=1;bi.memory_map=(uintptr_t)&desc;bi.memory_map_bytes=sizeof(desc);bi.map_buffer=(struct syle_range){(uintptr_t)&desc,sizeof(desc)};assert(pmm_init(&bi));
 if(fragment)for(unsigned i=0;i<PMM_MAX_RANGES*2+1;i++){ordinary[i]=pmm_alloc_page();assert(ordinary[i]);}
 struct vtc_entry entries[2]={0};struct vsa a={.storage=entries,.capacity=2};assert(vsa_begin(&a));struct vsc_ops o={&s,host_buffer,transfer};assert(vsc_init(&s,&a,&o)&&!vsc_init(&s,&a,&o));struct vlm_allocation page;assert(vsc_allocate(&s,SG_MEMORY_RAM,&page));assert(!pna_release(a.owned.epoch,page));active_page=page;
 if(fragment){
  for(unsigned i=0;i<PMM_MAX_RANGES;i++)assert(pmm_free_page(ordinary[2*i]));
  uint64_t count=pmm_get_stats().allocated_pages;assert(vsc_drain(&s));
  assert(!vsc_retire_after_quiescence(&s)&&!s.ended&&entries[0].live&&!entries[0].pinned&&pmm_get_stats().allocated_pages==count);
  uint64_t pa;assert(pna_page(a.owned.epoch,page,&pa));
  assert(pmm_free_page(ordinary[PMM_MAX_RANGES*2-1])&&pmm_free_page(ordinary[PMM_MAX_RANGES*2]));
  assert(vsc_retire_after_quiescence(&s));
  for(unsigned i=0;i<PMM_MAX_RANGES-1;i++)assert(pmm_free_page(ordinary[2*i+1]));
  assert(!pmm_get_stats().allocated_pages);puts("bounded retire actual PMM extent exhaustion: failed free retains owner, coalesce/retry PASS");return 0;
 }
 uint64_t saved_epoch=s.epoch;s.epoch++;assert(!vsc_copy(&s,page,VLM_GUEST_RAM,0,external,1,true)&&!transfers);s.epoch=saved_epoch;
 memset(external,0x5a,sizeof external);assert(!vsc_copy(&s,page,VLM_EPT_TABLE,0,external,1,true));assert(!vsc_copy(&s,page,VLM_GUEST_RAM,4095,external,2,true));assert(!vsc_copy(&s,page,VLM_GUEST_RAM,SIZE_MAX,external,1,true));assert(!vsc_copy(&s,page,VLM_GUEST_RAM,0,backing,1,true)&&!transfers);
 struct vlm_allocation stale=page;stale.generation++;assert(!vsc_copy(&s,stale,VLM_GUEST_RAM,0,external,1,true));s.borrowers=UINT_MAX;assert(!vsc_copy(&s,page,VLM_GUEST_RAM,0,external,1,true)&&s.borrowers==UINT_MAX);s.borrowers=0;
 if(argc==2&&(!strcmp(argv[1],"fatal-buffer")||!strcmp(argv[1],"fatal-transfer"))){
 fatal_phase=!strcmp(argv[1],"fatal-buffer")?1:2;
 if(!setjmp(fatal_jump)){(void)vsc_copy(&s,page,VLM_GUEST_RAM,0,external,16,true);assert(!"fatal unexpectedly returned");}
 assert(s.busy&&s.borrowers==1&&entries[0].pinned&&entries[0].live&&!s.ended&&pmm_get_stats().allocated_pages==1);
 /* Observe only and exit: no synthetic unpin/cleanup/resume after fatal. */
 puts("bounded operation fatal nonreturn: borrowed owner/pin retained PASS; longjmp observation only, not an NMI test");return 0;
 }
 if(argc==2&&!strcmp(argv[1],"buffer")){
 close_in_buffer=true;assert(vsc_copy(&s,page,VLM_GUEST_RAM,0,external,1,true)&&s.closing&&transfers==1&&!s.borrowers);
 }else if(argc==2){partial_fail=true;assert(!vsc_copy(&s,page,VLM_GUEST_RAM,0,external,16,true)&&s.faulted&&!s.borrowers&&!s.busy&&backing[0][0]==0x5a);assert(!vsc_copy(&s,page,VLM_GUEST_RAM,0,external,16,true)&&transfers==1);assert(vsc_drain(&s));}
 else{assert(vsc_copy(&s,page,VLM_GUEST_RAM,4095,external,1,true)&&backing[0][4095]==0x5a);memset(external,0,sizeof external);assert(vsc_copy(&s,page,VLM_GUEST_RAM,4095,external,1,false)&&external[0]==0x5a);close_inside=true;assert(vsc_copy(&s,page,VLM_GUEST_RAM,0,external,1,true)&&s.closing&&!s.borrowers);assert(!vsc_copy(&s,page,VLM_GUEST_RAM,0,external,1,true));}
 assert(vsc_retire_after_quiescence(&s)&&!pmm_get_stats().allocated_pages);assert(!vsc_copy(&s,page,VLM_GUEST_RAM,0,external,1,true));puts("bounded operation + actual PMM pin: bounds/type/stale/borrow-drain/partial-failure retention PASS; synchronous host only, no native NMI/DMA proof");return 0;
}
