#include "linux_vcpu_state.h"
const uint32_t lvs_msr_ids[LVS_MSR_COUNT]={0xc0000081u,0xc0000082u,
 0xc0000083u,0xc0000084u,0xc0000102u,0xc0000103u};
static bool aligned(const struct lvs_state *s){return s&&!((uintptr_t)s&15);}
static bool ops_ok(const struct lvs_ops *o){
 return o&&o->save_fx&&o->load_fx&&o->read_msr&&o->write_msr&&o->enter;
}
static bool same(const struct lvs_ops *a,const struct lvs_ops *b){
 return a->ctx==b->ctx&&a->save_fx==b->save_fx&&a->load_fx==b->load_fx&&
 a->read_msr==b->read_msr&&a->write_msr==b->write_msr&&a->enter==b->enter;
}
bool lvs_init(struct lvs_state *s){
 if(!aligned(s)||s->ready||s->host_dirty)return false;
 for(unsigned i=0;i<512;i++)s->guest.fx[i]=0;
 for(unsigned i=0;i<LVS_MSR_COUNT;i++)s->guest.msr[i]=0;
 s->guest.fx[0]=0x7f;s->guest.fx[1]=3;
 s->guest.fx[24]=0x80;s->guest.fx[25]=0x1f;
 s->ready=true;s->guest_valid=true;return true;
}
static bool capture(struct lvs_image *im,const struct lvs_ops *o){
 if(!o->save_fx(o->ctx,im->fx))return false;
 for(unsigned i=0;i<LVS_MSR_COUNT;i++)
  if(!o->read_msr(o->ctx,lvs_msr_ids[i],&im->msr[i]))return false;
 return true;
}
bool lvs_restore_host(struct lvs_state *s,const struct lvs_ops *o){
 if(!aligned(s)||!ops_ok(o)||!s->ready)return false;
 if(!s->host_dirty)return true;
 if(!same(&s->owner,o))return false;
 bool ok=true;
 for(unsigned i=0;i<LVS_MSR_COUNT;i++)
  if(!o->write_msr(o->ctx,lvs_msr_ids[i],s->host.msr[i]))ok=false;
 if(!o->load_fx(o->ctx,s->host.fx))ok=false;
 if(ok)s->host_dirty=false;
 return ok;
}
enum lvs_result lvs_run(struct lvs_state *s,const struct lvs_ops *o){
 if(!aligned(s)||!ops_ok(o)||!s->ready||!s->guest_valid||s->host_dirty)
  return LVS_BAD_STATE;
 /* Copy callbacks before invoking them; identical owner required on retries. */
 struct lvs_ops run=*o;
 if(!capture(&s->host,&run))return LVS_CAPTURE_FAILED;
 s->owner=run;s->host_dirty=true;
 enum lvs_result result=LVS_ENTRY_FAILED;
 if(!run.load_fx(run.ctx,s->guest.fx))goto restore;
 for(unsigned i=0;i<LVS_MSR_COUNT;i++)
  if(!run.write_msr(run.ctx,lvs_msr_ids[i],s->guest.msr[i]))goto restore;
 if(!run.enter(run.ctx))goto restore;
 s->guest_valid=false;
 if(!capture(&s->guest,&run)){result=LVS_GUEST_CAPTURE_FAILED;goto restore;}
 s->guest_valid=true;result=LVS_EXIT;
restore:
 if(!lvs_restore_host(s,&run))return LVS_HOST_DIRTY;
 return result;
}
