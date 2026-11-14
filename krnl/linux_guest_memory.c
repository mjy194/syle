#include "linux_guest_memory.h"
#define PHYS_MASK UINT64_C(0x000ffffffffff000)
static void wipe(void*p,size_t n){unsigned char*b=p;while(n--)*b++=0;}
static bool fresh(const struct linux_guest_memory*m){
 if(m->prepared||m->eptp)return false;
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++)if(m->pages[i])return false;
 for(unsigned i=0;i<LGM_TABLE_COUNT;i++)if(m->tables[i])return false;
 return true;
}
bool lgm_release(struct linux_guest_memory*m){
 if(!m)return false;
 if(!m->owner.release)return fresh(m);
 m->prepared=false;m->eptp=0;wipe(&m->plan,sizeof m->plan);wipe(&m->entry,sizeof m->entry);
 bool ok=true;
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++)if(m->pages[i]){
  if(m->owner.release(m->owner.context,m->pages[i]))m->pages[i]=0;else ok=false;
 }
 for(unsigned i=0;i<LGM_TABLE_COUNT;i++)if(m->tables[i]){
  if(m->owner.release(m->owner.context,m->tables[i]))m->tables[i]=0;else ok=false;
 }
 if(ok)wipe(&m->owner,sizeof m->owner);
 return ok;
}
static void*owned(struct linux_guest_memory*m,uint64_t*token,enum sg_memory_role role,sg_allocate_role_fn allocate_role){
 const struct sg_memory_ops*o=&m->owner;
 *token=allocate_role?allocate_role(o->context,role):o->allocate(o->context);
 if(!*token||(*token&~PHYS_MASK)||!o->writeback(o->context,*token))return 0;
 void*p=o->map(o->context,*token);
 if(!p||((uintptr_t)p&7))return 0;
 wipe(p,4096);return p;
}
static void*map(struct linux_guest_memory*m,uint64_t pa){
 void*p=m->owner.map(m->owner.context,pa);
 return p&&!((uintptr_t)p&7)?p:0;
}
static bool copy(void*ctx,uint64_t gpa,const void*src,size_t n){
 struct linux_guest_memory*m=ctx;const unsigned char*s=src;
 if(gpa>LGM_RAM_SIZE||n>LGM_RAM_SIZE-gpa||(!src&&n))return false;
 while(n){size_t off=gpa&4095,k=4096-off;if(k>n)k=n;
  unsigned char*p=map(m,m->pages[gpa/4096]);if(!p)return false;
  for(size_t i=0;i<k;i++)p[off+i]=s[i];
  s+=k;gpa+=k;n-=k;
 }return true;
}
static bool zero(void*ctx,uint64_t gpa,size_t n){
 struct linux_guest_memory*m=ctx;
 if(gpa>LGM_RAM_SIZE||n>LGM_RAM_SIZE-gpa)return false;
 while(n){size_t off=gpa&4095,k=4096-off;if(k>n)k=n;
  unsigned char*p=map(m,m->pages[gpa/4096]);if(!p)return false;
  wipe(p+off,k);gpa+=k;n-=k;
 }return true;
}
static bool prepare(struct linux_guest_memory*m,const struct sg_memory_ops*o,sg_allocate_role_fn allocate_role,
 const void*image,size_t size,const void*initrd,size_t initrd_size,const char*cmd){
 struct lb_plan plan;struct lb_entry_state entry;
 if(!m||!o||(!allocate_role&&!o->allocate)||!o->release||!o->map||!o->writeback||!fresh(m)||!initrd||!initrd_size)return false;
 if(!lb_inspect(image,size,initrd_size,cmd,&plan))return false;
 m->owner=*o;
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++)if(!owned(m,&m->pages[i],SG_MEMORY_RAM,allocate_role))goto fail;
 for(unsigned i=0;i<LGM_TABLE_COUNT;i++)if(!owned(m,&m->tables[i],SG_MEMORY_EPT,allocate_role))goto fail;
 struct lb_writer writer={m,copy,zero};
 if(!lb_load(image,size,initrd,initrd_size,cmd,&writer,&plan)||!lb_entry_prepare(&plan,&writer,&entry))goto fail;
 for(unsigned i=0;i<2;i++){
  uint64_t*p=map(m,m->tables[i]);if(!p)goto fail;p[0]=m->tables[i+1]|7;
 }
 uint64_t*pd=map(m,m->tables[2]);if(!pd)goto fail;
 for(unsigned i=0;i<LGM_PT_COUNT;i++){
  uint64_t*pt=map(m,m->tables[3+i]);if(!pt)goto fail;
  pd[i]=m->tables[3+i]|7;
  for(unsigned j=0;j<512;j++)pt[j]=m->pages[i*512+j]|0x37;
 }
 m->plan=plan;m->entry=entry;m->eptp=m->tables[0]|0x1e;m->prepared=true;return true;
fail:
 (void)lgm_release(m);return false;
}
bool lgm_prepare(struct linux_guest_memory*m,const struct sg_memory_ops*o,
 const void*image,size_t size,const void*initrd,size_t initrd_size,const char*cmd){
 return prepare(m,o,0,image,size,initrd,initrd_size,cmd);
}
bool lgm_prepare_roles(struct linux_guest_memory*m,const struct sg_memory_ops*o,sg_allocate_role_fn allocate_role,
 const void*image,size_t size,const void*initrd,size_t initrd_size,const char*cmd){
 return allocate_role&&prepare(m,o,allocate_role,image,size,initrd,initrd_size,cmd);
}
