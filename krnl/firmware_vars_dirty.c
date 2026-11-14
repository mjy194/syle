#include "firmware_vars_dirty.h"
#include "guest_mmio_mov.h"
static const unsigned regs_map[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
static uint64_t little(const uint8_t*p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=(uint64_t)p[i]<<(i*8);return v;}
static bool walk(const struct fmm_memory*m,uint64_t va,struct fvd_entry*out,unsigned*count,bool data){
 uint64_t table=m->cr3&UINT64_C(0xfffff000);*count=0;
 for(unsigned level=0;level<4;level++){
  if(table>FWM_RAM_SIZE-4096)return false;
  uint64_t token=m->memory->ram[table/4096];if(!fwm_owned_page(m->memory,token))return false;
  for(unsigned i=0;i<level;i++)if(token==out[i].token)return false;
  uint64_t address=table+((va>>(39-9*level))&511)*8;uint8_t bytes[8];
  if(!fwm_read_ram(m->memory,address,bytes,8))return false;
  uint64_t value=little(bytes);out[(*count)++]=(struct fvd_entry){address,value,token};
  if((value&0x21)!=0x21||(value&UINT64_C(0xffffffff00000000))||((value&128)&&level<2))return false;
  if(data&&((value&0x27)!=0x23))return false;
  if(level==3||((value&128)&&level==2)){
   if(level==2&&(value&UINT64_C(0x1fe000)))return false;
   return !data||level==2;
  }
  table=value&UINT64_C(0xfffff000);
 }
 return false;
}
static bool ram_pointer(struct firmware_memory*f,uint64_t gpa,uint64_t**pointer,uint64_t*token){
 if(!f||!f->prepared||f->quarantined||!f->owner.map||gpa>=FWM_RAM_SIZE||(gpa&7)||f->eptp!=(f->tables[0]|0x1e))return false;
 uint64_t t=f->ram[gpa/4096];if(!fwm_owned_page(f,t))return false;
 for(unsigned i=0;i<FWM_RAM_PAGES;i++)if(i!=gpa/4096&&f->ram[i]==t)return false;
 for(unsigned i=0;i<FWM_FLASH_PAGES;i++)if(f->flash[i]==t)return false;
 for(unsigned i=0;i<FWM_TABLE_PAGES;i++)if(f->tables[i]==t)return false;
 unsigned slots[]={0,1,2,4+(unsigned)(gpa>>21)},shift[]={39,30,21,12};
 for(unsigned level=0;level<4;level++){
  uint64_t pa=f->tables[slots[level]];if(!fwm_owned_page(f,pa))return false;
  for(unsigned i=0;i<level;i++)if(pa==f->tables[slots[i]])return false;
  const uint8_t*p=f->owner.map(f->owner.context,pa);if(!p||((uintptr_t)p&4095))return false;
  uint64_t value=little(p+((gpa>>shift[level])&511)*8);
  if(value!=(level==3?(t|0x37):(f->tables[slots[level+1]]|7)))return false;
 }
 uint8_t*p=f->owner.map(f->owner.context,t);if(!p||((uintptr_t)p&4095))return false;
 *pointer=(uint64_t*)(void*)(p+(gpa&4095));*token=t;return true;
}
static bool source_token(const struct firmware_memory*f,uint64_t gpa,uint64_t*out){
 if(gpa<FWM_RAM_SIZE)*out=f->ram[gpa/4096];
 else {if(!fwl_contains(&f->layout.code,gpa,1)||gpa<f->layout.vars.base)return false;
 uint64_t i=(gpa-f->layout.vars.base)/4096;if(i>=f->flash_pages||i>=FWM_FLASH_PAGES)return false;*out=f->flash[i];}
 return fwm_owned_page(f,*out);
}
bool fvd_capture(const struct fmm_memory*m,const struct fat_exit*e,const uint64_t g[15],const struct fvr_state*s,struct fvd_plan*out){
 if(!m||!m->memory||!e||!g||!s||!out||!m->advanced_ept_info||e->qualification!=0x58a||e->reason!=48||
 e->ds_base||e->es_base||e->ss_base||e->blocking||e->pending_debug||e->vectoring||e->entry_event||e->dr7!=0x400||
 !(e->rflags&2)||(e->rflags&~UINT64_C(0x250ed7)))return false;
 struct fvd_plan p={0};p.live_memory=m;p.live_exit=e;p.live_gpr=g;p.live_probe=s;p.memory=*m;p.exit=*e;p.probe=*s;p.owner=m->memory->owner;p.layout=m->memory->layout;p.eptp=m->memory->eptp;
 uint64_t regs[16];for(unsigned i=0;i<15;i++)p.gpr[i]=g[i];for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:g[regs_map[i]];
 uint8_t bytes[15];size_t n=fmm_fetch(m,e->rip,bytes);struct guest_mmio_mov mov;
 if(!n||!guest_mmio_mov_decode(bytes,n,e->rip,regs,&mov)||!mov.write||mov.width!=1||mov.linear!=e->linear)return false;
 uint64_t physical;if(!fmm_translate(m,mov.linear,FMM_READ,&physical)||physical!=e->physical)return false;
 struct fvr_state staged=*s;if(!fvr_probe(&staged,m->memory,physical,1,mov.store_value))return false;
 unsigned count;struct fvd_entry data[4];if(!walk(m,mov.linear,data,&count,true)||count!=3)return false;
 for(unsigned i=0;i<3;i++)p.data[i]=data[i];
 if(!ram_pointer(m->memory,p.data[2].gpa,&p.leaf_pointer,&p.leaf_token))return false;
 p.length=mov.length;
 for(unsigned i=0;i<mov.length;i++){
  p.instruction[i]=bytes[i];uint64_t src;
  if(!fmm_translate(m,e->rip+i,FMM_EXEC,&src)||!source_token(m->memory,src,&p.source_tokens[i])||p.source_tokens[i]==p.leaf_token)return false;
  struct fvd_entry entries[4];if(!walk(m,e->rip+i,entries,&count,false))return false;
  for(unsigned j=0;j<count;j++){
   if(entries[j].gpa==p.data[2].gpa)return false; /* reject ambiguous source-leaf role */
   if(entries[j].token==p.leaf_token&&((entries[j].gpa&~UINT64_C(4095))!=(p.data[2].gpa&~UINT64_C(4095))))return false;
   if(p.source_count>=60)return false;
   p.source[p.source_count++]=entries[j];
  }
 }
 p.transaction=(struct fwd_plan){p.data[2].gpa,p.data[2].value,e->rip,mov.next_rip,e->rflags};
 *out=p;return true;
}
static bool same_mode(const struct fmm_memory*a,const struct fmm_memory*b){return a->memory==b->memory&&a->cr0==b->cr0&&a->cr3==b->cr3&&a->cr4==b->cr4&&a->efer==b->efer&&a->cs_base==b->cs_base&&a->cs_ar==b->cs_ar&&a->cs_selector==b->cs_selector&&a->advanced_ept_info==b->advanced_ept_info;}
static bool same_region(const struct fwl_region*a,const struct fwl_region*b){return a->base==b->base&&a->size==b->size&&a->kind==b->kind&&a->read==b->read&&a->write==b->write&&a->execute==b->execute;}
static bool same_exit(const struct fat_exit*a,const struct fat_exit*b){return a->rip==b->rip&&a->rsp==b->rsp&&a->qualification==b->qualification&&a->linear==b->linear&&a->physical==b->physical&&a->ds_base==b->ds_base&&a->es_base==b->es_base&&a->ss_base==b->ss_base&&a->rflags==b->rflags&&a->blocking==b->blocking&&a->pending_debug==b->pending_debug&&a->dr7==b->dr7&&a->reason==b->reason&&a->vectoring==b->vectoring&&a->entry_event==b->entry_event;}
bool fvd_validate(const struct fvd_plan*p,bool after){
 if(!p||!p->live_memory||!p->live_exit||!p->live_gpr||!p->live_probe||!same_mode(&p->memory,p->live_memory)||!same_exit(&p->exit,p->live_exit))return false;
 const struct fvr_state*s=p->live_probe;if(s->address!=p->probe.address||s->original!=p->probe.original||s->stage!=p->probe.stage||s->ready!=p->probe.ready||s->failed!=p->probe.failed)return false;
 for(unsigned i=0;i<15;i++)if(p->gpr[i]!=p->live_gpr[i])return false;
 const struct firmware_memory*f=p->live_memory->memory;
 if(f->owner.context!=p->owner.context||f->owner.map!=p->owner.map||f->owner.allocate!=p->owner.allocate||f->owner.release!=p->owner.release||f->owner.writeback!=p->owner.writeback||f->eptp!=p->eptp||!same_region(&f->layout.ram,&p->layout.ram)||!same_region(&f->layout.vars,&p->layout.vars)||!same_region(&f->layout.code,&p->layout.code)||f->layout.reset_linear!=p->layout.reset_linear||f->layout.reset_code_offset!=p->layout.reset_code_offset)return false;
 struct fvd_plan now;if(!fvd_capture(p->live_memory,p->live_exit,p->live_gpr,p->live_probe,&now)||now.leaf_pointer!=p->leaf_pointer||now.leaf_token!=p->leaf_token||now.length!=p->length||now.source_count!=p->source_count)return false;
 if(now.transaction.leaf_entry_gpa!=p->transaction.leaf_entry_gpa||
 now.transaction.leaf_value!=(p->transaction.leaf_value|(after?0x40:0))||
 now.transaction.rip!=p->transaction.rip||now.transaction.next_rip!=p->transaction.next_rip||now.transaction.rflags!=p->transaction.rflags)return false;
 for(unsigned i=0;i<3;i++)if(now.data[i].gpa!=p->data[i].gpa||now.data[i].token!=p->data[i].token||now.data[i].value!=(p->data[i].value|((after&&i==2)?0x40:0)))return false;
 for(unsigned i=0;i<p->length;i++)if(now.instruction[i]!=p->instruction[i]||now.source_tokens[i]!=p->source_tokens[i])return false;
 for(unsigned i=0;i<p->source_count;i++)if(now.source[i].gpa!=p->source[i].gpa||now.source[i].token!=p->source[i].token||now.source[i].value!=p->source[i].value)return false;
 return !after||fmm_validate_vars_write(p->live_memory,p->exit.linear,1,48,0x58a,p->exit.linear,p->exit.physical,0);
}
enum fwd_cas_result fvd_compare_set(const struct fvd_plan*p,void*ctx,enum fwd_cas_result(*cas)(void*,uint64_t*,uint64_t,uint64_t)){
 if(!p||!cas||(p->transaction.leaf_value&0x40)||!fvd_validate(p,false))return FWD_CAS_CONFLICT;
 return cas(ctx,p->leaf_pointer,p->transaction.leaf_value,p->transaction.leaf_value|0x40);
}
