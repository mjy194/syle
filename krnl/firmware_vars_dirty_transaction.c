#include "firmware_vars_dirty_transaction.h"
static bool capture(void*x,struct fwd_plan*out){struct fvdt_state*s=x;if(!fvd_validate(&s->plan,false))return false;*out=s->plan.transaction;return true;}
static bool bind(const struct fwd_plan*a,const struct fwd_plan*b){return a->leaf_entry_gpa==b->leaf_entry_gpa&&a->leaf_value==b->leaf_value&&a->rip==b->rip&&a->next_rip==b->next_rip&&a->rflags==b->rflags;}
static bool validate(void*x,const struct fwd_plan*p,bool after){struct fvdt_state*s=x;return bind(p,&s->plan.transaction)&&fvd_validate(&s->plan,after);}
static enum fwd_cas_result compare(void*x,uint64_t address,uint64_t old,uint64_t desired){struct fvdt_state*s=x;if(address!=s->plan.transaction.leaf_entry_gpa||old!=s->plan.transaction.leaf_value||desired!=(old|0x40))return FWD_CAS_CONFLICT;return fvd_compare_set(&s->plan,s->callback_context,s->cas);}
static bool sample(void*x){struct fvdt_state*s=x;return s->sample(s->callback_context);}
static bool write_vm(void*x,uint32_t field,uint64_t value){struct fvdt_state*s=x;return s->write(s->callback_context,field,value);}
enum fat_result fvdt_apply(struct fvdt_state*s,struct fvr_state*vars,uint64_t g[15],
 const struct fmm_memory*m,const struct fat_exit*e,bool exclusive,bool c2,void*ctx,
 bool(*sample_fn)(void*),enum fwd_cas_result(*cas_fn)(void*,uint64_t*,uint64_t,uint64_t),
 bool(*write_fn)(void*,uint32_t,uint64_t)){
 if(!exclusive||!s||!vars)return FAT_REJECTED;
 if(s->transaction.failed){vars->failed=true;return FAT_FATAL;}
 if(s->transaction.consumed||vars->failed||!c2||!sample_fn||!cas_fn||!write_fn)return FAT_REJECTED;
 if(!fvd_capture(m,e,g,vars,&s->plan)||(s->plan.transaction.leaf_value&0x40))return FAT_REJECTED;
 s->staged=*vars;
 if(!fvr_probe(&s->staged,m->memory,vars->address,1,vars->stage?0x70:0x50))return FAT_REJECTED;
 s->callback_context=ctx;s->sample=sample_fn;s->cas=cas_fn;s->write=write_fn;
 const struct fwd_ops ops={capture,validate,compare,sample,write_vm};
 if(!fwd_apply(&s->transaction,true,true,s,&ops)){vars->failed=true;return FAT_FATAL;}
 *vars=s->staged;return FAT_COMMITTED;
}
