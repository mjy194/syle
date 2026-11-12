#include "nested_atomic_uc_provision.h"
static bool same_plan(const struct nauc_plan *a,const struct nauc_plan *b)
{
 if(a->nonce!=b->nonce||a->type_epoch!=b->type_epoch||a->target_hpa!=b->target_hpa||a->target_gpa!=b->target_gpa||a->host_pte!=b->host_pte||a->host_wb_word!=b->host_wb_word||a->host_uc_word!=b->host_uc_word||a->host_lease!=b->host_lease||a->inventory!=b->inventory)return false;
 for(unsigned i=0;i<NAUC_ROOTS;i++)if(a->eptp[i]!=b->eptp[i])return false;
 for(unsigned i=0;i<NAUC_CPUS;i++)if(a->cpu[i]!=b->cpu[i])return false;
 return true;
}
static bool valid(const struct nauc_plan *p)
{
 if(!p->nonce||!p->type_epoch||!p->target_hpa||(p->target_hpa&4095)||!p->target_gpa||(p->target_gpa&4095)||!p->host_pte||(p->host_pte&7)||!p->host_lease||!p->inventory||!(p->host_wb_word&1)||!(p->host_uc_word&1)||p->host_wb_word==p->host_uc_word||p->cpu[0]==p->cpu[1]||p->cpu[0]==NAUC_GLOBAL||p->cpu[1]==NAUC_GLOBAL)return false;
 if((p->host_wb_word&UINT64_C(0x000ffffffffff000))!=p->target_hpa||
    (p->host_wb_word&UINT64_C(0x8000000000000083))!=UINT64_C(0x8000000000000003)||
    (p->host_wb_word&0x18)||p->host_uc_word!=(p->host_wb_word|UINT64_C(0x18)))return false;
 for(unsigned i=0;i<NAUC_ROOTS;i++){
  /* Physical 4-level EPTP, WB page tables, AD disabled (fixed parking/EPT01 inventory); no reserved lows. */
  if(!(p->eptp[i]&~UINT64_C(4095))||(p->eptp[i]&4095)!=0x1e)return false;
  for(unsigned j=0;j<i;j++)if(p->eptp[i]==p->eptp[j])return false;
 }
 return true;
}
static bool step(struct nauc_controller *c,nauc_execute_fn fn,void *ctx,
 enum nauc_phase phase,uint32_t cpu,uint64_t eptp,uint32_t proofs)
{
 struct nauc_request q={.plan=c->plan,.ordinal=c->completed_actions+1,
  .eptp=eptp,.phase=phase,.cpu=cpu,.required_proofs=proofs|NAUC_GATE_CLOSED};
 struct nauc_receipt r={0};
 bool ok=fn(ctx,&q,&r);
 if(!ok||c->state!=NAUC_RUNNING||!r.completed||!same_plan(&q.plan,&r.action.plan)||
    !same_plan(&q.plan,&c->plan)||r.action.ordinal!=q.ordinal||r.action.eptp!=q.eptp||
    r.action.phase!=q.phase||r.action.cpu!=q.cpu||r.action.required_proofs!=q.required_proofs||
    (r.proofs&q.required_proofs)!=q.required_proofs){c->state=NAUC_POISONED;return false;}
 c->completed_actions=q.ordinal;c->last_completed=phase;return true;
}
static bool both(struct nauc_controller *c,nauc_execute_fn f,void *x,enum nauc_phase p,uint32_t proofs)
{
 for(unsigned i=0;i<NAUC_CPUS;i++)if(!step(c,f,x,p,c->plan.cpu[i],0,proofs))return false;
 return true;
}
static bool roots(struct nauc_controller *c,nauc_execute_fn f,void *x,enum nauc_phase p)
{
 for(unsigned i=0;i<NAUC_CPUS;i++)for(unsigned j=0;j<NAUC_ROOTS;j++)
  if(!step(c,f,x,p,c->plan.cpu[i],c->plan.eptp[j],0))return false;
 return true;
}
bool nauc_run(struct nauc_controller *c,const struct nauc_plan *p,nauc_execute_fn f,void *x)
{
 if(!c)return false;
 if(c->state!=NAUC_NEW){c->state=NAUC_POISONED;return false;}
 c->state=NAUC_POISONED;
 if(!p||!f||!valid(p))return false;
 c->plan=*p;c->state=NAUC_RUNNING;c->retention_attempted=true;
 if(!step(c,f,x,NAUC_RETAIN,NAUC_GLOBAL,0,NAUC_RETAINED|NAUC_UNPUBLISHED))return false;
 c->retained=true;
 if(!both(c,f,x,NAUC_PREFLIGHT,NAUC_RETAINED|NAUC_STAGED|NAUC_PAT_MTRR_WB|NAUC_ROOT_OWNER|NAUC_GUEST_NEVER_ENTERED|NAUC_UNPUBLISHED)||
    !step(c,f,x,NAUC_DETACH,NAUC_GLOBAL,0,NAUC_RETAINED)||
    !roots(c,f,x,NAUC_INVEPT_BEFORE)||
    !step(c,f,x,NAUC_HOST_BREAK,NAUC_GLOBAL,0,0)||
    !both(c,f,x,NAUC_INVLPG_BEFORE,0)||!both(c,f,x,NAUC_WBINVD_BEFORE,0)||
    !step(c,f,x,NAUC_HOST_MAKE,NAUC_GLOBAL,0,0)||
    !both(c,f,x,NAUC_INVLPG_AFTER,0)||!both(c,f,x,NAUC_WBINVD_AFTER,0)||
    !both(c,f,x,NAUC_AUDIT_UC,NAUC_CURRENT_UC|NAUC_ROOT_OWNER|NAUC_GUEST_NEVER_ENTERED)||
    !step(c,f,x,NAUC_INSTALL,NAUC_GLOBAL,0,NAUC_UNPUBLISHED)||
    !roots(c,f,x,NAUC_INVEPT_AFTER)||
    !both(c,f,x,NAUC_AUDIT_FINAL,NAUC_FINAL_SOURCE_HOST_ALIASES|NAUC_UNPUBLISHED|NAUC_CURRENT_UC|NAUC_ROOT_OWNER|NAUC_GUEST_NEVER_ENTERED)||
    !step(c,f,x,NAUC_PUBLISH,NAUC_GLOBAL,0,NAUC_RETAINED))return false;
 c->state=NAUC_PUBLISHED;return true;
}
