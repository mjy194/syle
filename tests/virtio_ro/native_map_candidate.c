#include "native_map_candidate.h"
#include "../../krnl/paging.h"
#include "../../krnl/vmx_memory.h"
#include "../../efi/src/exceptions.h"
static bool gate(void){
#ifdef VNMAP_HOST_TEST
 extern bool vnm_test_context(void);
 return vnm_test_context();
#else
 /* No caller boolean can bypass the missing actual NMI-entry marker. NMI
  * from user mode currently reaches guest_owner_exit before fatal-stop. */
 panic("SCOPED NATIVE MAP BLOCKED: NMI CONTEXT GATE NOT INTEGRATED");
 return false;
#endif
}
static bool span(uintptr_t a,size_t n){return n&&a&&n<=UINTPTR_MAX-a;}
static bool overlap(uintptr_t a,size_t n,uintptr_t b,size_t z){return a<b+z&&b<a+n;}
static bool live(struct vnm *m){return m&&m->configured&&m->scope&&m->scope->initialized&&!m->scope->ended&&m->scope->busy&&m->scope->borrowers==1&&m->scope->epoch==m->scope->authority->owned.epoch&&m->root==paging_root();}
static bool host_buffer(void *ctx,const void *buffer,size_t n){
 struct vnm *m=ctx;uintptr_t p=(uintptr_t)buffer;
 if(!gate()||!live(m)||!span(p,n)||p<m->external||n>m->external_size||p-m->external>m->external_size-n)return false;
 struct vsa *a=m->scope->authority;
 return !overlap(p,n,(uintptr_t)m,sizeof(*m))&&!overlap(p,n,(uintptr_t)m->scope,sizeof(*m->scope))&&
 !overlap(p,n,(uintptr_t)a,sizeof(*a))&&!overlap(p,n,(uintptr_t)a->owned.entries,(size_t)a->owned.capacity*sizeof(*a->owned.entries));
}
static bool transfer(void *ctx,uint64_t pa,size_t offset,void *buffer,size_t n,bool write){
 struct vnm *m=ctx;enum vlm_kind kind;
 if(!host_buffer(ctx,buffer,n)||offset>4096||n>4096-offset||!vsa_query(m->scope->authority,pa,&kind)||
 !paging_page_rw_wb(pa)||!vmx_memory_region_wb(pa))return false;
 /* Physical translation is consumed only inside this synchronous operation.
  * Page tables/PAT/MTRRs must remain stable; host tests do not prove TOCTOU. */
 unsigned char *view=paging_phys_to_virt(pa);uintptr_t v=(uintptr_t)view;
 if(!span(v,4096)||overlap(v,4096,(uintptr_t)buffer,n)||m->root!=paging_root())return false;
 unsigned char *p=buffer;
 for(size_t i=0;i<n;i++){if(write)view[offset+i]=p[i];else p[i]=view[offset+i];}
 __asm__ volatile("" ::: "memory");
 return true;
}
bool vnm_bind(struct vnm *m,struct vsc *s,void *external,size_t n,struct vsc_ops *out){
 if(!m||m->configured||!s||!out||!span((uintptr_t)external,n)||!paging_root())return false;
 *m=(struct vnm){.scope=s,.external=(uintptr_t)external,.external_size=n,.root=paging_root(),.configured=true};
 *out=(struct vsc_ops){m,host_buffer,transfer};return true;
}
