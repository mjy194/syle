#include "session_scope.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
static struct {uint32_t type,pad;uint64_t base,virt,pages,attr;} desc={7,0,0x100000,0,2,8};
static struct css c,newc;
static struct css_handle h,newh;
static struct vlm_allocation a;
static unsigned char src[4096],bytes[4096];
static unsigned callbacks,writes,fatal_phase;
static bool reject,partial,drain_inside,unpin_fail;
static jmp_buf fatal;
bool __real_pna_unpin(uint64_t,struct vlm_handle);
bool __wrap_pna_unpin(uint64_t e,struct vlm_handle l){return !unpin_fail&&__real_pna_unpin(e,l);}
static bool source(void*ctx,const void*p,size_t n){
 struct css*s=ctx;callbacks++;assert(s->busy&&s->borrows==1&&s->pinned);
 assert(!pna_release(s->epoch,a));
 assert(!css_write(s,(struct css_handle){s,s->epoch},a,VLM_GUEST_RAM,0,src,1));
 if(fatal_phase==1)longjmp(fatal,1);
 if(drain_inside)assert(!css_close(s,(struct css_handle){s,s->epoch}));
 return !reject&&p==src&&n<=sizeof src;
}
static bool transfer(void*ctx,uint64_t pa,size_t off,const void*p,size_t n){
 struct css*s=ctx;assert(s->busy&&s->borrows==1&&s->pinned&&pa==0x100000);writes++;
 if(fatal_phase==2)longjmp(fatal,1);
 memcpy(bytes+off,p,partial?1:n);return !partial;
}
static void setup(void){struct syle_boot_info bi={0};bi.magic=SYLE_BOOT_MAGIC;bi.version=SYLE_BOOT_VERSION;bi.size=sizeof bi;bi.boot_services_exited=1;bi.descriptor_size=sizeof desc;bi.descriptor_version=1;bi.memory_map=(uintptr_t)&desc;bi.memory_map_bytes=sizeof desc;bi.map_buffer=(struct syle_range){(uintptr_t)&desc,sizeof desc};assert(pmm_init(&bi));}
int main(int argc,char**argv){
 assert(argc==1||argc==2);setup();uint64_t epoch;assert(pna_start(&epoch)&&pna_alloc_typed(epoch,VLM_GUEST_RAM,&a));
 struct css_ops ops={&c,source,transfer};assert(css_init(&c,epoch,&ops,&h));struct css_handle sentinel={0,77};assert(!css_init(&c,epoch,&ops,&sentinel)&&!sentinel.issuer&&sentinel.epoch==77);
 memset(src,0x5a,sizeof src);
 if(argc==2){
  if(!strcmp(argv[1],"fatal-source"))fatal_phase=1;
  else if(!strcmp(argv[1],"fatal-transfer"))fatal_phase=2;
  else if(!strcmp(argv[1],"unpin"))unpin_fail=true;
  else if(!strcmp(argv[1],"reject-unpin")){unpin_fail=true;reject=true;}
  else if(!strcmp(argv[1],"partial"))partial=true;
  else if(!strcmp(argv[1],"drain"))drain_inside=true;
  else assert(0);
  if(fatal_phase){
   if(!setjmp(fatal)){(void)css_write(&c,h,a,VLM_GUEST_RAM,0,src,16);assert(!"fatal returned");}
   assert(c.busy&&c.borrows==1&&c.pinned&&!pna_release(epoch,a)&&!css_close(&c,h));
   puts("host nonreturn observation retains exact lease/borrow; NOT NMI PASS");return 0;
  }
  bool ok=css_write(&c,h,a,VLM_GUEST_RAM,0,src,16);
  if(unpin_fail){assert(!ok&&c.faulted&&c.busy&&c.borrows==1&&c.pinned&&!pna_release(epoch,a)&&!css_close(&c,h));puts("unpin failure retains exact lease/terminal busy PASS");return 0;}
  if(partial){assert(!ok&&c.faulted&&bytes[0]==0x5a&&bytes[1]==0);assert(!css_write(&c,h,a,VLM_GUEST_RAM,0,src,1));}
  else {assert(ok&&c.closing);assert(!css_write(&c,h,a,VLM_GUEST_RAM,0,src,1));}
  assert(!c.busy&&!c.borrows&&!c.pinned&&css_close(&c,h)&&pna_release(epoch,a)&&pna_drain(epoch)&&pna_end(epoch));puts("partial/drain completed-copy close PASS");return 0;
 }
 assert(!css_write(&c,h,a,VLM_EPT_TABLE,0,src,1));assert(!css_write(&c,h,a,VLM_GUEST_RAM,SIZE_MAX,src,1));assert(!css_write(&c,h,a,VLM_GUEST_RAM,4095,src,2));assert(!css_write(&c,h,a,VLM_GUEST_RAM,0,src,0));assert(!callbacks);
 struct vlm_handle existing;assert(pna_pin(epoch,a,&existing));
 assert(!css_write(&c,h,a,VLM_GUEST_RAM,0,src,1)&&!callbacks);assert(pna_unpin(epoch,existing));
 reject=true;assert(!css_write(&c,h,a,VLM_GUEST_RAM,0,src,1)&&!writes&&!c.faulted&&!c.pinned);reject=false;
 assert(css_write(&c,h,a,VLM_GUEST_RAM,4095,src,1)&&bytes[4095]==0x5a);
 struct vlm_allocation stale=a;uint64_t pa;assert(pna_page(epoch,a,&pa)&&pa==0x100000&&pna_release(epoch,a));assert(pna_alloc_typed(epoch,VLM_GUEST_RAM,&a)&&pna_page(epoch,a,&pa)&&pa==0x100000);
 unsigned before=callbacks;assert(!css_write(&c,h,stale,VLM_GUEST_RAM,0,src,1)&&callbacks==before);assert(css_write(&c,h,a,VLM_GUEST_RAM,0,src,1));
 stale=a;assert(css_close(&c,h)&&pna_release(epoch,a)&&pna_drain(epoch)&&pna_end(epoch));
 uint64_t newer;assert(pna_start(&newer)&&newer!=epoch&&pna_alloc_typed(newer,VLM_GUEST_RAM,&a)&&pna_page(newer,a,&pa)&&pa==0x100000);
 ops.ctx=&newc;assert(css_init(&newc,newer,&ops,&newh));before=callbacks;
 assert(!css_write(&newc,h,a,VLM_GUEST_RAM,0,src,1));assert(!css_write(&c,h,a,VLM_GUEST_RAM,0,src,1));assert(!css_write(&newc,newh,stale,VLM_GUEST_RAM,0,src,1)&&callbacks==before);
 assert(css_write(&newc,newh,a,VLM_GUEST_RAM,0,src,1));assert(css_close(&newc,newh)&&pna_release(newer,a)&&pna_drain(newer)&&pna_end(newer));
 puts("session handle + same/cross epoch allocation ABA + scoped source PASS");return 0;
}
