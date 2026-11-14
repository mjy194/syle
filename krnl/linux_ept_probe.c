#include "linux_ept_probe.h"
extern const unsigned char lep_code_start[],lep_code_end[],lep_read[],lep_read_done[],lep_write[],lep_write_done[];
extern const unsigned char lep_exec_start[],lep_exec_end[],lep_exec_done[];
#define PMASK UINT64_C(0x000ffffffffff000)
static bool bad(struct lep_context*c){if(c)c->failed=true;return false;}
static bool overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t k){
 if(a>UINT64_MAX-n||b>UINT64_MAX-k)return true;
 return a<b+k&&b<a+n;
}
static bool read_field(struct lgs_session*s,uint32_t f,uint64_t*v){return s->platform.read_field(s->platform.ctx,f,v);}
static void*owned_map(struct lgs_session*s,uint64_t pa){
 if(!pa||(pa&4095)||(pa&~PMASK)||!s->memory.owner.map)return 0;
 return s->memory.owner.map(s->memory.owner.context,pa);
}
static bool unchanged_tables(struct lgs_session*s){
 struct linux_guest_memory*m=&s->memory;
 uint64_t *p=owned_map(s,m->tables[0]);if(!p||((uintptr_t)p&7)||p[0]!=(m->tables[1]|7))return false;
 p=owned_map(s,m->tables[1]);if(!p||((uintptr_t)p&7)||p[0]!=(m->tables[2]|7))return false;
 p=owned_map(s,m->tables[2]);if(!p||((uintptr_t)p&7)||p[0]!=(m->tables[3]|7))return false;
 return m->eptp==(m->tables[0]|0x1e);
}
static bool guest_identity(struct lgs_session*s){
 uint64_t*p=owned_map(s,s->memory.pages[1]);if(!p||((uintptr_t)p&7)||p[0]!=0x2003)return false;
 p=owned_map(s,s->memory.pages[2]);if(!p||((uintptr_t)p&7)||p[0]!=0x3003)return false;
 p=owned_map(s,s->memory.pages[3]);return p&&!((uintptr_t)p&7)&&p[0]==0x83;
}
bool lep_reset(void*x){struct lep_context*c=x;if(!c)return false;
 /* Service reset only occurs before fresh CREATE, after prior destruction. */
 *c=(struct lep_context){0};return true;}
bool lep_prepare(void*x,struct lgs_session*s){
 struct lep_context*c=x;
 if(!c||!s||c->failed||c->prepared||s->enabled||s->host.active||s->native.resume||
 !s->memory.prepared||s->memory.plan.ram_size!=LGM_RAM_SIZE||!unchanged_tables(s)||!guest_identity(s))return bad(c);
 const struct lb_plan*p=&s->memory.plan;
 /* Entire probe range is below the Linux image and outside every bootstrap
  * object. It is never used by concurrently running Linux: none has entered. */
 if(overlap(LEP_CODE,0x3000,p->kernel_addr,p->kernel_envelope)||
 overlap(LEP_CODE,0x3000,p->initrd_addr,p->initrd_size)||
 overlap(LEP_CODE,0x3000,p->params_addr,4096)||overlap(LEP_CODE,0x3000,p->cmdline_addr,4096)||
 overlap(LEP_CODE,0x3000,0x1000,0x6000)||overlap(LEP_CODE,0x3000,0x7f000,4096))return bad(c);
 uintptr_t code_size=(uintptr_t)lep_code_end-(uintptr_t)lep_code_start;
 uintptr_t exec_size=(uintptr_t)lep_exec_end-(uintptr_t)lep_exec_start;
 if(!code_size||code_size>4096||!exec_size||exec_size>4096)return bad(c);
 uint64_t*pt=owned_map(s,s->memory.tables[3]);if(!pt||((uintptr_t)pt&7))return bad(c);
 unsigned char*pages[3];
 for(unsigned i=0;i<3;i++){
  unsigned index=(unsigned)(LEP_CODE/4096)+i;uint64_t pa=s->memory.pages[index];
  if(pt[index]!=(pa|0x37))return bad(c);
  /* Trusted allocator contract still applies globally; defend selected pages
   * against a corrupt local mapping before writing the probe. */
  for(unsigned j=0;j<LGM_TABLE_COUNT;j++)if(pa==s->memory.tables[j])return bad(c);
  for(unsigned j=0;j<LGM_PAGE_COUNT;j++)if(j!=index&&pa==s->memory.pages[j])return bad(c);
  pages[i]=owned_map(s,pa);if(!pages[i]||((uintptr_t)pages[i]&7))return bad(c);
 }
 for(unsigned i=0;i<3;i++)for(unsigned j=0;j<4096;j++)pages[i][j]=0;
 for(uintptr_t i=0;i<code_size;i++)pages[0][i]=lep_code_start[i];
 for(uintptr_t i=0;i<exec_size;i++)pages[2][i]=lep_exec_start[i];
 *(uint64_t*)pages[1]=LEP_INITIAL;
 c->data_leaf=&pt[LEP_DATA/4096];c->exec_leaf=&pt[LEP_EXEC/4096];
 c->data_original=*c->data_leaf;c->exec_original=*c->exec_leaf;
 c->read_rip=LEP_CODE+(uintptr_t)lep_read-(uintptr_t)lep_code_start;
 c->read_done=LEP_CODE+(uintptr_t)lep_read_done-(uintptr_t)lep_code_start;
 c->write_rip=LEP_CODE+(uintptr_t)lep_write-(uintptr_t)lep_code_start;
 c->write_done=LEP_CODE+(uintptr_t)lep_write_done-(uintptr_t)lep_code_start;
 c->exec_done=LEP_EXEC+(uintptr_t)lep_exec_done-(uintptr_t)lep_exec_start;
 if(c->read_rip>=LEP_CODE+code_size||c->read_done>=LEP_CODE+code_size||
 c->write_rip>=LEP_CODE+code_size||c->write_done>=LEP_CODE+code_size||c->exec_done>=LEP_EXEC+exec_size)return bad(c);
 *c->data_leaf=c->data_original&~UINT64_C(7); /* R=0 also requires W=0. */
 c->session=s;c->prepared=true;return true;
}
bool lep_before(void*x,struct lgs_session*s){
 struct lep_context*c=x;uint64_t info,vector,cr3,cs;
 if(!c||c->failed||!c->prepared||c->complete||c->session!=s||!s->hop.vmwrite||!s->hop.invept||
 !read_field(s,0x4016,&info)||info||
 (c->started&&(!read_field(s,0x4408,&vector)||vector))||
 !read_field(s,0x6802,&cr3)||cr3!=0x1000||!read_field(s,0x0802,&cs)||cs!=0x10)return bad(c);
 if(!c->started){
  if(s->exits||s->native.resume||!s->hop.vmwrite(s->hop.context,0x681e,LEP_CODE)||
   !s->hop.invept(s->hop.context,s->memory.eptp))return bad(c);
  c->started=true;
 }
 return true;
}
bool lep_after(void*x,struct lgs_session*s,const struct lgs_entry_result*r){
 struct lep_context*c=x;
 if(!c||c->failed||c->session!=s||!r||r->state_result!=LVS_EXIT||!r->fields_valid||r->vectoring)return bad(c);
 return true;
}
static bool change(struct lep_context*c,uint64_t*leaf,uint64_t old,uint64_t next){
 struct lgs_session*s=c->session;if(*leaf!=old)return bad(c);
 *leaf=next;
 /* Failure is sticky; never resume with uncertain EPT cache state. */
 if(!s->hop.invept(s->hop.context,s->memory.eptp))return bad(c);
 return true;
}
bool lep_dispatch(void*x,struct lgs_session*s,uint32_t reason){
 struct lep_context*c=x;uint64_t rip,vector,info,cs;
 if(!c||c->failed||!c->started||c->complete||c->session!=s||++c->exits>32||
 !read_field(s,0x681e,&rip)||!read_field(s,0x4408,&vector)||vector||
 !read_field(s,0x4016,&info)||info||!read_field(s,0x0802,&cs)||cs!=0x10)return bad(c);
 if(reason==52)return true; /* No RIP advancement, bounded by the exit budget. */
 if(reason==48){
  uint64_t q,gpa,gla;unsigned access=c->phase==0?1:c->phase==1?2:4;
  uint64_t expected_rip=c->phase==0?c->read_rip:c->phase==1?c->write_rip:LEP_EXEC;
  uint64_t target=c->phase<2?LEP_DATA:LEP_EXEC;
  uint64_t original=c->phase<2?c->data_original:c->exec_original;
  unsigned permissions=c->phase==0?0:c->phase==1?5:3;
  uint64_t*leaf=c->phase<2?c->data_leaf:c->exec_leaf;
  if(c->phase>2||c->repaired||rip!=expected_rip||
   !read_field(s,0x6400,&q)||!read_field(s,0x2400,&gpa)||gpa!=target||
   !read_field(s,0x640a,&gla)||gla!=target||
   (q&7)!=access||((q>>3)&7)!=permissions||(q&0x180)!=0x180||
   (q&~UINT64_C(0xfbf)))return bad(c);
  if(!change(c,leaf,(original&~UINT64_C(7))|permissions,original))return false;
  /* Do not write RIP: hardware retries the same guest instruction. */
  c->repaired=true;c->faults++;return true;
 }
 if(reason==18){
  uint64_t len;uint64_t target=c->phase==0?c->read_done:c->phase==1?c->write_done:c->exec_done;
  uint64_t expected=c->phase==0?LEP_INITIAL:c->phase==1?LEP_WRITTEN:LEP_EXEC_VALUE;
  if(c->phase>2||!c->repaired||rip!=target||s->native.gpr[0]!=c->phase+1||s->native.gpr[1]!=expected||
   !read_field(s,0x440c,&len)||len!=3)return bad(c);
  if(c->phase==1){uint64_t*p=owned_map(s,s->memory.pages[LEP_DATA/4096]);if(!p||*p!=LEP_WRITTEN)return bad(c);}
  if(c->phase==2){if(c->faults!=3)return bad(c);c->complete=true;return true;}
  uint64_t*leaf=c->phase==0?c->data_leaf:c->exec_leaf;
  uint64_t original=c->phase==0?c->data_original:c->exec_original;
  uint64_t perms=c->phase==0?5:3;
  if(!change(c,leaf,original,(original&~UINT64_C(7))|perms)||
   !s->hop.vmwrite(s->hop.context,0x681e,rip+3))return bad(c);
  c->phase++;c->repaired=false;return true;
 }
 return bad(c);
}
int lep_recv(void*x){struct lep_context*c=x;if(!c||c->failed)return -(int)SYLE_GUEST_EBACKEND;
 if(c->complete&&!c->published){c->published=true;return 'P';}return -(int)SYLE_GUEST_EBUSY;}
bool lep_init(struct lep_context*c,struct lgm_service_ops*o){
 if(!c||!o)return false;
 *c=(struct lep_context){0};
 *o=(struct lgm_service_ops){.ctx=c,.reset=lep_reset,.prepare_guest=lep_prepare,
 .before_entry=lep_before,.after_entry=lep_after,.dispatch=lep_dispatch,.recv=lep_recv};return true;
}
