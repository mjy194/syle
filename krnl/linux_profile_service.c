#include "linux_profile_service.h"
static bool released(const struct lgs_session*s){
 if(!s)return true;
 if(s->prepared||s->enabled||s->host.active||s->host.ready||s->host.quarantined||s->host.vmcs_page||s->host.vmxon_page||s->memory.prepared||s->state.host_dirty)return false;
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++)if(s->memory.pages[i])return false;
 for(unsigned i=0;i<LGM_TABLE_COUNT;i++)if(s->memory.tables[i])return false;
 return true;
}
static bool reset(void*ctx){
 struct lps_context*p=ctx;if(!p||!p->configured||p->busy||p->dispatch.busy||p->dispatch.entry_inflight||p->prepare.busy)return false;
 if(!released(p->prepare.session)||!released(p->dispatch.session)||p->generation==UINT64_MAX)return false;
 p->busy=true;
 /* Entire old domain is destroyed, so quarantine cannot refer to live state. */
 p->prepare=(struct lpp_context){0};p->dispatch=(struct lgd_context){0};p->generation++;
 bool ok=lpp_init(&p->prepare,p->generation,p->clock_ctx,p->now);
 if(ok){struct lgd_config c=p->base;struct lgm_service_ops unused;
 c.apic=p->prepare.config;c.irq_adapter=NULL;c.irq_delivery_enabled=c.irq_platform_ready=true;
 c.irq_ctx=&p->prepare.irq;c.sync_irq=gia_irq4;ok=lgd_init(&p->dispatch,&c,&unused);}
 p->failed=!ok;p->busy=false;return ok;
}
static bool prepare(void*ctx,struct lgs_session*s){struct lps_context*p=ctx;if(!p||p->busy||p->failed)return false;p->busy=true;bool ok=lpp_prepare_guest(&p->prepare,s);p->failed=!ok;p->busy=false;return ok;}
static bool dispatch(void*ctx,struct lgs_session*s,uint32_t r){struct lps_context*p=ctx;return p&&!p->busy&&!p->failed&&lgd_dispatch(&p->dispatch,s,r);}
static bool before(void*ctx,struct lgs_session*s){struct lps_context*p=ctx;return p&&!p->busy&&!p->failed&&lgd_before_entry(&p->dispatch,s);}
static bool after(void*ctx,struct lgs_session*s,const struct lgs_entry_result*r){struct lps_context*p=ctx;return p&&!p->busy&&!p->failed&&lgd_after_entry(&p->dispatch,s,r);}
static int send(void*ctx,uint8_t b){struct lps_context*p=ctx;return p&&!p->busy&&!p->failed?lgd_send(&p->dispatch,b):-16;}
static int recv(void*ctx){struct lps_context*p=ctx;return p&&!p->busy&&!p->failed?lgd_recv(&p->dispatch):-16;}
bool lps_init(struct lps_context*p,const struct lgd_config*c,void*x,bool(*now)(void*,uint64_t*),struct lgm_service_ops*out){
 if(!p||!c||!out||!now||p->configured||p->busy||p->generation)return false;
 p->base=*c;p->clock_ctx=x;p->now=now;p->configured=true;
 if(!reset(p))return false;
 *out=(struct lgm_service_ops){.ctx=p,.reset=reset,.dispatch=dispatch,.send=send,.recv=recv,.before_entry=before,.after_entry=after,.prepare_guest=prepare};return true;
}
