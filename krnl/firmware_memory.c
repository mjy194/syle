#include "firmware_memory.h"
#define PAGE UINT64_C(4096)
_Static_assert(FWM_HASH_SLOTS==65536u,"hash width must match fixed table");
_Static_assert(FWM_RAM_PAGES+FWM_FLASH_PAGES+FWM_TABLE_PAGES<FWM_HASH_SLOTS,"owned set capacity");
static void wipe(void*p,size_t n){unsigned char*b=p;while(n--)*b++=0;}
static void bytes(void*d,const void*s,size_t n){unsigned char*a=d;const unsigned char*b=s;while(n--)*a++=*b++;}
static unsigned hash(uint64_t p){return (unsigned)(((p>>12)*UINT64_C(11400714819323198485))>>(64-16));}
bool fwm_owned_page(const struct firmware_memory*m,uint64_t p){
 if(!m||!p||(p&(PAGE-1)))return false;
 unsigned h=hash(p);
 for(unsigned n=0;n<FWM_HASH_SLOTS;n++,h=(h+1)&(FWM_HASH_SLOTS-1)){
  if(!m->owned_hash[h])return false;
  if(m->owned_hash[h]==p)return true;
 }return false;
}
static bool record(struct firmware_memory*m,uint64_t p){
 unsigned h=hash(p);
 for(unsigned n=0;n<FWM_HASH_SLOTS;n++,h=(h+1)&(FWM_HASH_SLOTS-1)){
  if(m->owned_hash[h]==p)return false;
  if(!m->owned_hash[h]){m->owned_hash[h]=p;return true;}
 }return false;
}
static bool fresh(const struct firmware_memory*m){
 if(m->prepared||m->quarantined||m->eptp||m->rejected_token||m->flash_pages||m->physical_bits)return false;
 for(unsigned i=0;i<FWM_RAM_PAGES;i++)if(m->ram[i])return false;
 for(unsigned i=0;i<FWM_FLASH_PAGES;i++)if(m->flash[i])return false;
 for(unsigned i=0;i<FWM_TABLE_PAGES;i++)if(m->tables[i])return false;
 for(unsigned i=0;i<FWM_HASH_SLOTS;i++)if(m->owned_hash[i])return false;
 return true;
}
static void*map(struct firmware_memory*m,uint64_t p){
 void*v=m->owner.map(m->owner.context,p);
 return v&&!((uintptr_t)v&(PAGE-1))?v:0;
}
static void*alloc(struct firmware_memory*m,uint64_t*slot){
 uint64_t p=m->owner.allocate(m->owner.context);
 if(!p)return 0;
 if((p&(PAGE-1))||p>((UINT64_C(1)<<m->physical_bits)-PAGE)||!record(m,p)){
  m->quarantined=true;m->rejected_token=p;return 0;
 }
 *slot=p;
 if(!m->owner.writeback(m->owner.context,p))return 0;
 void*v=map(m,p);if(!v)return 0;
 wipe(v,PAGE);return v;
}
bool fwm_release(struct firmware_memory*m){
 if(!m)return false;
 if(m->quarantined)return false;
 m->prepared=false;m->eptp=0;
 if(!m->owner.release)return fresh(m);
 bool ok=true;
 for(unsigned i=0;i<FWM_RAM_PAGES;i++)if(m->ram[i]){
  if(m->owner.release(m->owner.context,m->ram[i]))m->ram[i]=0;else ok=false;
 }
 for(unsigned i=0;i<FWM_FLASH_PAGES;i++)if(m->flash[i]){
  if(m->owner.release(m->owner.context,m->flash[i]))m->flash[i]=0;else ok=false;
 }
 for(unsigned i=0;i<FWM_TABLE_PAGES;i++)if(m->tables[i]){
  if(m->owner.release(m->owner.context,m->tables[i]))m->tables[i]=0;else ok=false;
 }
 /* Rebuild membership after partial release. Removing an open-addressed key
  * in place would break probe chains, so rebuild only from retained tokens. */
 wipe(m->owned_hash,sizeof m->owned_hash);
 for(unsigned i=0;i<FWM_RAM_PAGES;i++)if(m->ram[i])(void)record(m,m->ram[i]);
 for(unsigned i=0;i<FWM_FLASH_PAGES;i++)if(m->flash[i])(void)record(m,m->flash[i]);
 for(unsigned i=0;i<FWM_TABLE_PAGES;i++)if(m->tables[i])(void)record(m,m->tables[i]);
 if(ok){wipe(&m->owner,sizeof m->owner);wipe(&m->layout,sizeof m->layout);m->flash_pages=0;m->physical_bits=0;}
 return ok;
}
static bool ram_copy(struct firmware_memory*m,uint64_t a,void*buffer,size_t n,bool write){
 if(!m||!m->prepared||m->quarantined||!buffer||!n||a>FWM_RAM_SIZE||n>FWM_RAM_SIZE-a)return false;
 unsigned char*b=buffer;
 while(n){unsigned off=a&(PAGE-1);size_t k=PAGE-off;if(k>n)k=n;
  unsigned char*p=map(m,m->ram[a/PAGE]);if(!p)return false;
  if(write)bytes(p+off,b,k);else bytes(b,p+off,k);
  a+=k;b+=k;n-=k;
 }return true;
}
bool fwm_read_ram(struct firmware_memory*m,uint64_t a,void*b,size_t n){return ram_copy(m,a,b,n,false);}
bool fwm_write_ram(struct firmware_memory*m,uint64_t a,const void*b,size_t n){return ram_copy(m,a,(void*)b,n,true);}
bool fwm_prepare(struct firmware_memory*m,const struct sg_memory_ops*o,
 const void*code,size_t code_size,const void*vars,size_t vars_size,unsigned physical_bits){
 const struct fwl_request request={FWM_RAM_SIZE,code_size,vars_size,physical_bits};
 const struct fwl_request*r=&request;struct fwl_plan p;
 if(!m||!o||!o->allocate||!o->release||!o->map||!o->writeback||!fresh(m)||
 !code||!vars||r->ram_size!=FWM_RAM_SIZE||
 !fwl_plan_build(r,&p)||r->code_size>FWM_FLASH_MAX||r->vars_size>FWM_FLASH_MAX-r->code_size)return false;
 m->owner=*o;m->layout=p;m->physical_bits=r->physical_bits;
 m->flash_pages=(unsigned)((r->code_size+r->vars_size)/PAGE);
 for(unsigned i=0;i<FWM_RAM_PAGES;i++)if(!alloc(m,&m->ram[i]))goto fail;
 for(unsigned i=0;i<m->flash_pages;i++){
  void*v=alloc(m,&m->flash[i]);if(!v)goto fail;
  uint64_t off=(uint64_t)i*PAGE;
  if(off<r->vars_size)bytes(v,(const unsigned char*)vars+off,PAGE);
  else bytes(v,(const unsigned char*)code+off-r->vars_size,PAGE);
 }
 for(unsigned i=0;i<FWM_TABLE_PAGES;i++)if(!alloc(m,&m->tables[i]))goto fail;
 uint64_t*root=map(m,m->tables[0]),*pdpt=map(m,m->tables[1]);
 uint64_t*lo=map(m,m->tables[2]),*hi=map(m,m->tables[3]);
 if(!root||!pdpt||!lo||!hi)goto fail;
 root[0]=m->tables[1]|7;pdpt[0]=m->tables[2]|7;pdpt[3]=m->tables[3]|7;
 for(unsigned i=0;i<64;i++){
  uint64_t*pt=map(m,m->tables[4+i]);if(!pt)goto fail;lo[i]=m->tables[4+i]|7;
  for(unsigned j=0;j<512;j++)pt[j]=m->ram[i*512+j]|0x37;
 }
 /* Flash occupies a suffix of the final 4MiB. Only populated PT parents are
  * installed; the rest of high GPA and every intermediate hole stays absent. */
 for(unsigned i=0;i<m->flash_pages;i++){
  uint64_t gpa=p.vars.base+(uint64_t)i*PAGE;
  unsigned di=(unsigned)((gpa>>21)&511),ti=(unsigned)((gpa>>12)&511);
  if(di<510)goto fail;
  unsigned table=68+(di-510);uint64_t*pt=map(m,m->tables[table]);if(!pt)goto fail;
  hi[di]=m->tables[table]|7;
  pt[ti]=m->flash[i]|(gpa<p.code.base?0x31:0x35);
 }
 m->eptp=m->tables[0]|0x1e;m->prepared=true;return true;
fail:
 (void)fwm_release(m);return false;
}
