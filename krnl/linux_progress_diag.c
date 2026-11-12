#include "linux_progress_diag.h"
bool lprog_due(struct lprog_gate*g,uint64_t exits,bool after,bool success){
 if(!g)return false;
 if(after){if(!g->awaiting)return false;g->awaiting=false;if(!success||exits!=g->expected||g->records>=131)return false;g->records++;return true;}
 if(g->awaiting||g->samples>=64||g->records>=131||exits==UINT64_MAX||(exits+1)%4096)return false;
 g->expected=exits+1;g->samples++;g->records++;g->awaiting=true;return true;
}
bool lprog_mark(struct lprog_gate*g,unsigned bits){
 if(!g||g->records>=131||!(bits&7&~g->seen))return false;
 g->seen|=bits&7;g->records++;return true;
}
bool lprog_collect(const struct lgd_context*d,struct lgs_session*s,struct lprog_snapshot*out){
 if(!d||!s||!out||!d->configured||!d->ready||d->busy||d->entry_inflight||s->state.host_dirty||!s->state.ready||!s->host.active||!s->host.ready||!s->prepared||!s->platform.safe||!s->platform.safe(s->platform.ctx)||!s->platform.read_field||(d->session&&d->session!=s))return false;
 struct lprog_snapshot v={.exits=s->exits,.ticks=d->ticks,.stage=s->step_stage,.result=s->step_result};
 if(d->config.legacy_pic_enabled&&d->legacy_initialized){uint16_t count;
  v.reload=d->legacy.timer.timer.reload;v.edges=d->legacy.timer.timer.edges;
  v.timer_valid=pit0_count(&d->legacy.timer.timer,&count);if(v.timer_valid)v.count=count;
  v.irr=d->legacy.timer.pic.irr;v.isr=d->legacy.timer.pic.isr;v.reserved=d->legacy.timer.pic.reserved;v.phase=d->legacy.irq.event.phase;
 }
 bool a=s->platform.read_field(s->platform.ctx,0x4402,&v.reason);bool b=s->platform.read_field(s->platform.ctx,0x681e,&v.rip);v.vmcs_valid=a&&b;
 *out=v;return true;
}
