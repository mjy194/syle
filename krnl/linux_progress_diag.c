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
bool lprog_irq4_once(bool*sent,bool accepted){if(!sent||*sent||!accepted)return false;*sent=true;return true;}
bool lprog_collect(const struct lgd_context*d,struct lgs_session*s,struct lprog_snapshot*out){
 if(!d||!s||!out||!d->configured||!d->ready||d->busy||d->entry_inflight||s->state.host_dirty||!s->state.ready||!s->host.active||!s->host.ready||!s->prepared||!s->platform.safe||!s->platform.safe(s->platform.ctx)||!s->platform.read_field||(d->session&&d->session!=s))return false;
 struct lprog_snapshot v={.dispatch_stage=d->diagnostic_stage,.pf_pending=d->pf.pending,.pf_count=d->pf.count,.high_state=d->high,.demand_pending=d->demand_pf.pending,.demand_count=d->demand_pf.count,.demand_failed=d->demand_pf.failed,.exits=s->exits,.ticks=d->ticks,.stage=s->step_stage,.result=s->step_result};
 if(d->config.legacy_pic_enabled&&d->legacy_initialized){uint16_t count;
  v.reload=d->legacy.timer.timer.reload;v.edges=d->legacy.timer.timer.edges;
  v.timer_valid=pit0_count(&d->legacy.timer.timer,&count);if(v.timer_valid)v.count=count;
  v.irr=d->legacy.timer.pic.irr;v.isr=d->legacy.timer.pic.isr;v.reserved=d->legacy.timer.pic.reserved;v.phase=d->legacy.irq.event.phase;
 }
 if(d->config.legacy_pic04_enabled){uint16_t count;
  v.reload=d->pic04.timer.reload;v.edges=d->pic04.timer.edges;
  v.timer_valid=pit0_count(&d->pic04.timer,&count);if(v.timer_valid)v.count=count;
  v.irr=d->pic04.master.irr;v.isr=d->pic04.master.isr;v.reserved=d->pic04.master.reserved.serial;v.phase=d->pic04_run.irq.event.phase;
  v.irq_source=d->pic04.master.reserved.serial?d->pic04.master.reserved.source:UINT64_MAX;
  v.receipt_source=d->pic04.master.completed.serial?d->pic04.master.completed.source:UINT64_MAX;
  v.uart_level=d->pic04.master.level4;v.uart_dirty=d->pic04.uart.irq_dirty;v.uart_edges=d->pic04.master.seen4;v.idle_reason=d->pic04_idle_reason;
  const struct linux_uart*u=&d->pic04.uart.uart;v.uart_rx_count=u->rc;v.uart_rx_idle=u->rx_idle;v.uart_fcr=u->fcr;v.uart_ier=u->ier;v.uart_mcr=u->mcr;v.uart_lcr=u->lcr;v.uart_divisor=u->dll|((uint64_t)u->dlm<<8);
 }
 bool a=s->platform.read_field(s->platform.ctx,0x4402,&v.reason);bool b=s->platform.read_field(s->platform.ctx,0x681e,&v.rip);v.vmcs_valid=a&&b;
 *out=v;return true;
}
