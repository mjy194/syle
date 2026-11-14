#include "linux_host_clock_hv.h"
static bool properties(const struct lhch_ops*o,uint32_t*cpu){
 uint32_t r[4];
 if(!o||!o->cpuid||!o->reference||!o->cpuid(o->ctx,1,r)||!(r[2]&(1u<<31)))return false;
 *cpu=r[1]>>24;
 if(!o->cpuid(o->ctx,0x40000000,r)||r[0]<0x40000003)return false;
 if(!o->cpuid(o->ctx,0x40000001,r)||r[0]!=0x31237648)return false; /* Hv#1 */
 if(!o->cpuid(o->ctx,0x40000003,r)||!(r[0]&2))return false; /* reference counter access */
 return true;
}
static bool read_sample(void*p,struct lhc_sample*out){
 struct lhch_context*c=p;uint32_t cpu;uint64_t value;
 if(!c||!c->admitted||!out||!properties(&c->ops,&cpu)||cpu!=c->cpu||!c->ops.reference(c->ops.ctx,&value))return false;
 *out=(struct lhc_sample){value,c->epoch,cpu};return true;
}
bool lhch_admit(struct lhch_context*c,const struct lhch_ops*o,uint64_t epoch,struct lhcr_source*out){
 uint32_t cpu;uint64_t a,b;
 if(!c||!out||!epoch||!properties(o,&cpu)||!o->reference(o->ctx,&a)||!o->reference(o->ctx,&b)||b<a)return false;
 *c=(struct lhch_context){.ops=*o,.cpu=cpu,.epoch=epoch,.admitted=true};
 /* Hyper-V time-reference count is specified in 100ns, not the TSC rate.
  * Invariant here describes the interface's fixed units while available. */
 *out=(struct lhcr_source){.properties={10000000,epoch,cpu,true,true},.max_gap=600000000,.ctx=c,.sample=read_sample};return true;
}
