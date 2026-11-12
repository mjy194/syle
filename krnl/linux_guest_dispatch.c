#include "linux_guest_dispatch.h"
#include "linux_guest_manager.h"
#include "linux_guest_snapshot.h"
#include "linux_cpu_commit.h"
#include "linux_vcpu_cr.h"
#include "linux_gs_commit.h"
#include "linux_efer_commit.h"
#include "linux_msr_bank.h"
#include "linux_guest_irq_glue.h"
#include "linux_disabled_apic.h"
#include "linux_invlpg.h"
bool lgd_apic_now(void*p,uint64_t*out){
 struct lgd_context*d=p;if(!d||!out||d->failed||d->ticks>UINT64_MAX/1000||
 (d->clock_staged&&(!d->busy||d->staged_tick!=d->ticks+1||d->staged_tick>UINT64_MAX/1000)))return false;
 *out=(d->clock_staged?d->staged_tick:d->ticks)*1000;return true;
}
static bool canonical(uint64_t x){return (x>>47)==0||(x>>47)==0x1ffff;}
static bool rd(struct lgs_session*s,uint32_t f,uint64_t*v){return s->platform.read_field(s->platform.ctx,f,v);}
static bool wr(struct lgs_session*s,uint32_t f,uint64_t v){return s->hop.vmwrite(s->hop.context,f,v);}
static bool clear_event(struct lgs_session*s){return wr(s,0x4016,0)&&wr(s,0x4018,0)&&wr(s,0x401a,0);}
static bool consume(struct lgd_context*d,struct lgs_session*s,uint64_t reason,uint64_t rip,uint64_t vector,uint64_t event){
 enum lhp_state high=d->high;struct lpfg_state next;struct lnp_state demand=d->demand_pf;
 if(demand.pending){
  if(d->pf.pending||high==LHP_PENDING||!lnp_observe(&demand,reason,vector,event,&demand))return false;
  event=0;
 }
 if(high==LHP_PENDING){
  if(d->pf.pending||!lhp_consume(high,reason,vector,event,&high))return false;
  event=0;
 }
 if(!lpfg_observe(&d->pf,reason,rip,vector,event,&next)||!clear_event(s))return false;
 d->high=high;d->pf=next;d->demand_pf=demand;return true;
}
static bool cr(struct lgs_session*s,uint64_t rip,uint64_t q,unsigned len){
 struct lvc_state v={.rip=rip};struct lvc_change c;
 for(unsigned i=0;i<15;i++)v.gpr[i]=s->native.gpr[i];
 if(!rd(s,0x681c,&v.rsp)||!rd(s,0x6800,&v.cr0)||!rd(s,0x6802,&v.cr3)||
    !rd(s,0x6804,&v.cr4)||!rd(s,0x6004,&v.cr0_shadow)||!rd(s,0x6006,&v.cr4_shadow))return false;
 const struct lvp_caps*p=&s->host.caps;
 struct lvc_limits l={p->cr0_fixed0,p->cr0_fixed1,p->cr4_fixed0,p->cr4_fixed1,LGM_RAM_SIZE};
 if(!lvc_emulate(&v,&l,q,len,&c))return false;
 if((c.changed&LVC_CR0)&&(!wr(s,0x6800,c.next.cr0)||!wr(s,0x6004,c.next.cr0_shadow)))return false;
 if((c.changed&LVC_CR3)&&!wr(s,0x6802,c.next.cr3))return false;
 if((c.changed&LVC_CR4)&&(!wr(s,0x6804,c.next.cr4)||!wr(s,0x6006,c.next.cr4_shadow)))return false;
 if((c.changed&LVC_RSP)&&!wr(s,0x681c,c.next.rsp))return false;
 if(!s->hop.invept(s->hop.context,s->memory.eptp)||!wr(s,0x681e,c.next.rip))return false;
 for(unsigned i=0;i<15;i++)s->native.gpr[i]=c.next.gpr[i];
 return true;
}
static bool pf(struct lgd_context*d,struct lgs_session*s,uint64_t reason,uint64_t rip,uint64_t q){
 if(!d->config.fixed_input_verified)return false;
 if(d->pf.closed&&d->high==LHP_CONSUMED)return lnp_inject(&d->demand_pf,s);
 if(d->pf.closed){
  struct lhp_snapshot snap;
  return lgds_high(s,reason,rip,q,0,0,&snap)&&lhp_accept(d->high,&d->pf,&snap)&&
   lhp_commit(&d->high,&s->native.cr2,&s->native.resume,q,s->hop.context,s->hop.vmwrite);
 }
 struct lve_pf_exit e={.reason=reason,.rip=rip,.qualification=q};
 struct lve_pf_entry inject;struct lpfg_state next;uint64_t activity,cr3;
 if(!rd(s,0x4404,&e.interruption_info)||!rd(s,0x4406,&e.interruption_error)||
    !rd(s,0x4824,&e.interruptibility)||!rd(s,0x4826,&activity)||!rd(s,0x6818,&e.idt_base)||
    !rd(s,0x4812,&e.idt_limit)||!rd(s,0x6802,&cr3)||!lpfg_inject(&d->pf,&e,cr3,activity,&next,&inject))return false;
 if(!wr(s,0x4018,inject.error)||!wr(s,0x401a,inject.instruction_length)||!wr(s,0x4016,inject.info))return false;
 s->native.cr2=inject.cr2;d->pf=next;return true;
}
static bool msr(struct lgd_context*d,struct lgs_session*s,uint64_t reason,uint64_t rip,unsigned len){
 uint32_t id=(uint32_t)s->native.gpr[2];uint64_t base;
 if(id==0x8b)return lmc_msr(&d->microcode,s->native.gpr,rip,0,reason,len,s->hop.context,s->hop.vmwrite);
 if(id==0xc0000101u)return rd(s,0x6810,&base)&&lgsc_apply(s->native.gpr,rip,base,0,reason,len,s->hop.context,s->hop.vmwrite);
 struct lvp_cpuid_result basic,ext;
 if(!lcp_query(d->activation.kind,&d->config.cpu,1,0,&basic)||!lcp_query(d->activation.kind,&d->config.cpu,0x80000001,0,&ext))return false;
 if(d->config.apic.transactions&&(id==0x1b||(id>=0x800&&id<=0x8ff)))
  return lad_msr(&d->config.apic,s,basic.c,basic.d)>=LAMS_COMMITTED;
 if(id==0x1b)return lda_read(d->activation.kind,basic.c,basic.d,s->native.gpr,rip,0,reason,len,s->hop.context,s->hop.vmwrite);
 if(id==0xc0000080u){
  struct lefer_env e={.eptp=s->memory.eptp,.ext_edx=ext.d};
  return rd(s,0x2806,&e.efer)&&rd(s,0x6800,&e.cr0)&&rd(s,0x6804,&e.cr4)&&rd(s,0x4012,&e.entry)&&
   lefer_apply(s->native.gpr,rip,0,reason,len,&e,s->hop.context,s->hop.vmwrite,s->hop.invept);
 }
 if(!rd(s,0x680e,&base))return false;
 return lmbr_apply(s->native.gpr,rip,0,reason,len,basic.d,ext.d,base,&s->state,s->hop.context,s->hop.vmwrite);
}
bool lgd_dispatch(void*p,struct lgs_session*s,uint32_t supplied){
 struct lgd_context*d=p;
 if(!d||!d->configured||!d->ready||d->failed||d->busy||d->entry_inflight)return false;
 d->busy=true;bool ok=false;
 if(!s||!s->prepared||s->failed||!s->state.ready||!s->state.guest_valid||s->state.host_dirty||
    !s->platform.read_field||!s->hop.vmwrite||!s->hop.invept||(d->session&&d->session!=s)||
    d->exits>=d->config.max_exits)goto done;
 if(d->config.apic.transactions&&(!d->activation.active||d->activation.failed||d->activation.session!=s))goto done;
 d->session=s;d->exits++;
 /* The final allowed observed exit is terminal: no event preparation,
  * emulation or true return that could authorize entry N+1. */
 if(d->exits==d->config.max_exits||d->ticks>=UINT64_MAX/1000||d->clock_staged)goto done;
 d->staged_tick=d->ticks+1;d->clock_staged=true;
 uint64_t reason,rip,q,len,cs,vector,event;
 if(!rd(s,0x4402,&reason)||reason!=supplied||reason>UINT16_MAX||!rd(s,0x681e,&rip)||
    !canonical(rip)||!rd(s,0x6400,&q)||!rd(s,0x440c,&len)||!rd(s,0x0802,&cs)||(cs&3)||
    !rd(s,0x4408,&vector)||vector||!rd(s,0x4016,&event)||!consume(d,s,reason,rip,vector,event))goto done;
 /* One bounded logical-level retry; never repeat committed RX/TX/guest I/O.
  * IRQ_PENDING remains dirty and does not claim external IRQ delivery. */
 if(d->uart.irq_dirty&&lurt_retry_irq(&d->uart)==LURT_REJECTED)goto done;
 /* Timer yield consumes pending events but never advances guest RIP. */
 if(reason==52){ok=true;goto done;}
 if(reason==0){ok=pf(d,s,reason,rip,q);goto done;}
 if(reason==48&&d->config.apic.transactions){
  struct lvp_cpuid_result cp;
  ok=lcp_query(d->activation.kind,&d->config.cpu,1,0,&cp)&&lad_mmio(&d->config.apic,s,cp.d)>=LAMS_COMMITTED;
  goto done;
 }
 if(len>15)goto done;
 switch(reason){
 case 14:ok=linv_apply(s,rip,q,(unsigned)len);break;
 case 28:ok=cr(s,rip,q,(unsigned)len);break;
 case 10:{uint32_t leaf=(uint32_t)s->native.gpr[0];
  ok=lcp_apply(d->activation.kind,&d->config.cpu,s->native.gpr,rip,0,reason,(unsigned)len,s->hop.context,s->hop.vmwrite);
  if(ok&&leaf==1)d->microcode.loaded=true;
  break;}
 case 16:ok=lpit2_rdtsc(&d->pit,s->native.gpr,rip,0,reason,(unsigned)len,d->staged_tick,s->hop.context,s->hop.vmwrite)==LPIT_COMMITTED;break;
 case 31:case 32:ok=msr(d,s,reason,rip,(unsigned)len);break;
 case 30:
  if((q>>16)==0x61||(q>>16)==0x42||(q>>16)==0x43){
   ok=lpit2_io(&d->pit,s->native.gpr,rip,0,reason,q,(unsigned)len,d->staged_tick,s->hop.context,s->hop.vmwrite)==LPIT_COMMITTED;break;
  }
  ok=lurt_guest_io(&d->uart,s->native.gpr,rip,0,reason,q,(unsigned)len,s->hop.context,s->hop.vmwrite)>=0;break;
 default:break;
 }
done:
 if(ok&&d->clock_staged)d->ticks=d->staged_tick;
 d->clock_staged=false;
 if(!ok){d->failed=true;d->activation.failed=true;}
 d->busy=false;return ok;
}
bool lgd_reset(void*p){
 struct lgd_context*d=p;if(!d||!d->configured||d->busy||d->entry_inflight||(d->session&&(d->session->prepared||d->session->enabled||d->session->host.active||d->session->memory.prepared)))return false;
 if(d->config.apic.transactions&&(!lad_valid(&d->config.apic)||d->config.apic.irq->reserved||d->config.apic.irq->quarantined||d->config.apic.irq->receipt_pending||d->config.apic.transactions->pending))return false;
 if(d->config.irq_adapter&&(d->config.irq_adapter->reserved||d->config.irq_adapter->quarantined))return false;
 struct linux_uart_runtime uart;
 if(!lurt_init(&uart,d->config.irq_ctx,d->config.sync_irq))return false;
 d->ticks=d->staged_tick=0;d->clock_staged=false;(void)lpit2_init(&d->pit,0);d->uart=uart;d->pf=(struct lpfg_state){0};d->demand_pf=(struct lnp_state){0};d->high=LHP_UNUSED;d->microcode=(struct lmc_state){0};d->session=NULL;
 d->activation=(struct lpa_state){0};d->irq=(struct lig_state){0};d->entry_inflight=false;d->exits=0;d->failed=false;d->ready=true;return true;
}
int lgd_send(void*p,uint8_t byte){
 struct lgd_context*d=p;if(!d||!d->ready||d->failed||d->busy||d->entry_inflight||(d->config.apic.transactions&&!d->activation.active))return -16;
 d->busy=true;bool accepted=false;enum lurt_result r=lurt_receive(&d->uart,byte,&accepted);d->busy=false;
 return r>=0&&accepted?0:-16;
}
int lgd_recv(void*p){
 struct lgd_context*d=p;if(!d||!d->ready||d->failed||d->busy||d->entry_inflight||(d->config.apic.transactions&&!d->activation.active))return -16;
 d->busy=true;bool available=false;uint8_t byte=0;enum lurt_result r=lurt_pop_tx(&d->uart,&available,&byte);d->busy=false;
 return r>=0&&available?byte:-16;
}
bool lgd_init(struct lgd_context*d,const struct lgd_config*c,struct lgm_service_ops*out){
 if(!d||!c||!out||d->configured||d->ready||d->busy||!c->sync_irq||!c->max_exits||c->max_exits>(1u<<20)||
    !lcpu_host_valid(c->max_basic,c->vendor_b,c->vendor_c,c->vendor_d,c->max_extended,&c->cpu))return false;
 struct lvp_cpuid_result cp;
 bool guest_apic=c->apic.transactions!=NULL;
 if(guest_apic&&(c->apic.now!=lgd_apic_now||c->apic.clock_ctx!=d||!lad_valid(&c->apic)||c->irq_adapter||!c->irq_delivery_enabled||!c->irq_platform_ready||
    c->sync_irq!=gia_irq4||c->irq_ctx!=c->apic.irq))return false;
 if(!guest_apic&&(c->apic.irq||c->apic.now))return false;
 if(c->irq_delivery_enabled&&!guest_apic&&((!c->irq_adapter||!c->irq_adapter->enabled)||!c->irq_platform_ready||
    !lvp_cpuid(&c->cpu,1,0,&cp)||!(cp.d&(1u<<9))||!(cp.c&(1u<<21))))return false;
 *d=(struct lgd_context){.config=*c,.configured=true};
 if(!lgd_reset(d))return false;
 *out=(struct lgm_service_ops){.ctx=d,.reset=lgd_reset,.dispatch=lgd_dispatch,.send=lgd_send,.recv=lgd_recv,.before_entry=lgd_before_entry,.after_entry=lgd_after_entry};return true;
}

bool lgd_before_entry(void*p,struct lgs_session*s){
 struct lgd_context*d=p;
 if(!d||!d->configured||!d->ready||d->failed||d->busy||d->entry_inflight)return false;
 d->busy=true;bool ok=false;
 if(!s||!s->prepared||s->failed||!s->state.ready||s->state.host_dirty||!s->state.guest_valid||
    (d->session&&d->session!=s)||d->exits>=d->config.max_exits)goto done;
 d->session=s;
 if(d->config.apic.transactions&&!lpa_before(&d->activation,&d->config.apic,s,&d->config.cpu))goto done;
 if(d->config.irq_delivery_enabled){
  struct lei_snapshot g={.pf_pending=d->pf.pending||d->demand_pf.pending,.high_pending=d->high==LHP_PENDING};
  uint64_t b,a,e,v,err,len;
  if(!s->platform.read_field||!s->hop.vmwrite||
     !rd(s,0x6820,&g.rflags)||!rd(s,0x4824,&b)||b>UINT32_MAX||
     !rd(s,0x4826,&a)||a>UINT32_MAX||!rd(s,0x4016,&e)||e>UINT32_MAX||
     !rd(s,0x4408,&v)||v>UINT32_MAX||!rd(s,0x4018,&err)||!rd(s,0x401a,&len))goto done;
  g.blocking=b;g.activity=a;g.entry_info=e;g.vectoring=v;
  if(!g.pf_pending&&!g.high_pending&&(err||len))goto done;
  if(d->config.apic.transactions){
   uint64_t now;
   if(d->config.apic.transactions->pending&&lams_retry(d->config.apic.transactions)!=LAMS_COMMITTED)goto done;
   if(!d->config.apic.now(d->config.apic.clock_ctx,&now)||!gia_poll(d->config.apic.irq,now)||
      !lgig_before(&d->irq,d->config.apic.irq,&g,s->hop.context,s->hop.vmwrite))goto done;
  }else if(!lig_before(&d->irq,d->config.irq_adapter,&g,s->hop.context,s->hop.vmwrite))goto done;
 }
 d->entry_inflight=true;ok=true;
done:if(!ok){d->failed=true;d->activation.failed=true;}d->busy=false;return ok;
}
bool lgd_after_entry(void*p,struct lgs_session*s,const struct lgs_entry_result*r){
 struct lgd_context*d=p;
 if(!d||!d->configured||!d->ready||d->failed||d->busy||!d->entry_inflight)return false;
 d->busy=true;d->entry_inflight=false;bool ok=false;
 if(!s||d->session!=s||!r||s->state.host_dirty)goto done;
 enum lig_outcome outcome=LIG_UNKNOWN;
 /* Proven paths in lvs_run: these return before enter, or enter returned
  * false under the native no-guest-instruction contract. Dirty host is fatal
  * in session and must NEVER reach this callback. */
 if(r->state_result==LVS_BAD_STATE||r->state_result==LVS_CAPTURE_FAILED||r->state_result==LVS_ENTRY_FAILED){
  if(r->fields_valid||r->reason||r->vectoring)goto done;
  outcome=LIG_NO_ENTRY;
 }else if(r->state_result==LVS_EXIT&&r->fields_valid)outcome=LIG_EXIT;
 if(d->config.irq_delivery_enabled){
  bool reconciled=d->config.apic.transactions?
   lgig_after(&d->irq,d->config.apic.irq,outcome,r->reason,r->vectoring,s->hop.context,s->hop.vmwrite):
   lig_after(&d->irq,d->config.irq_adapter,outcome,r->reason,r->vectoring,s->hop.context,s->hop.vmwrite);
  if(!reconciled)goto done;
 }
 if(outcome==LIG_UNKNOWN)goto done;
 ok=true;
done:if(!ok){d->failed=true;d->activation.failed=true;}d->busy=false;return ok;
}
