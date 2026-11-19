/* Each L1 CPU owns its VMCS02 and the EPT composed for its active EPTP12.
 * VMCS12 host fields and guest MSR-list addresses stay virtual. */
struct nested_vmcs_batch {
 struct {uint32_t field;uint64_t value;} entry[LVP_MAX_FIELDS];
 unsigned count;
};
static bool nested_batch_add(struct nested_vmcs_batch *b,uint32_t field,uint64_t value){
 if(b->count==LVP_MAX_FIELDS)return false;
 unsigned i=b->count++;
 b->entry[i].field=field;b->entry[i].value=value;return true;
}
static bool nested_batch_exec(struct nested_vmcs_batch *b,bool write){
#ifdef SYLE_M2_HOST_PARALLEL
 if(nested_current==&nested_contexts[1])return fmp_vmcs_batch(b,write);
#endif
 for(unsigned i=0;i<b->count;i++)
  if(write?!wr(b->entry[i].field,b->entry[i].value):!rd(b->entry[i].field,&b->entry[i].value))return false;
 return true;
}
static bool nested_page_alloc(void *ctx,struct nested_sparse_page *out){
 struct nested_page_budget *budget=ctx;
 if(budget->pages>=512)return false;
 uint64_t p=alloc(0);if(!p)return false;
 if(p==budget->vmcs||p==host.vmxon_page||p==host.vmcs_page||fwm_owned_page(&mem,p))fatal();
 void *v=map(0,p);
 if(!v||!wb(0,p)){if(!release(0,p))fatal();return false;}
 *out=(struct nested_sparse_page){p,v};budget->pages++;return true;
}
static void nested_page_free(void *ctx,struct nested_sparse_page page){
 struct nested_page_budget *budget=ctx;
 if(!budget->pages||!release(0,page.hpa))fatal();
 budget->pages--;
}
static bool nested_flush(void *ctx,uint64_t eptp){
 #ifdef SYLE_M2_HOST_PARALLEL
 return fmp_nested_invept(ctx==&nested_contexts[1].composed_budget,eptp);
#else
 (void)ctx;return ops.invept(ops.context,eptp);
#endif
}

/* L1 RAM remains pinned for the guest lifetime. Within one virtual EPTP
 * hardware may cache translations until INVEPT. No table survives a guest
 * stop, VMXOFF, VMCLEAR or backing release. */
static bool nested_clear(uint64_t page){
#ifdef SYLE_M2_HOST_PARALLEL
 if(nested_current==&nested_contexts[1])return fmp_call(FMP_CLEAR,page,0);
#endif
 return ops.vmclear(ops.context,page);
}
static void nested_drop_cpu(void){
 struct nested_l1_context *ctx=nested_current;
 if(ctx->cpu.page&&(!nested_clear(ctx->cpu.page)||
    !release(0,ctx->cpu.page)))fatal();
 ctx->cpu=(__typeof__(ctx->cpu)){0};
 ctx->composed_budget.vmcs=0;
}
static void nested_drop_ept(void){
 struct nested_l1_context *ctx=nested_current;
 if(ctx->composed.root&&!nested_ept_sparse_destroy(&ctx->composed))fatal();
 if(ctx->composed_budget.pages)fatal();
 ctx->composed_source=0;
}

static bool nested_ram_page(void *ctx,uint64_t gpa,uint8_t access,uint64_t *hpa,
                            const uint64_t **view,uint8_t *perms){
 (void)ctx;(void)access;
 if(gpa>=FWM_RAM_SIZE||(gpa&4095))return false;
 unsigned page=gpa>>12;
 uint64_t p=mem.ram[page];
 static const unsigned upper[]={0,1,2};
 const uint64_t *parent;
 for(unsigned i=0;i<3;i++){
  parent=map(0,mem.tables[upper[i]]);
  unsigned index=i==2?page>>9:0;
  uint64_t next=mem.tables[i==2?4+index:i+1];
  if(!parent||parent[index]!=(next|7))return false;
 }
 const uint64_t *pt=map(0,mem.tables[4+(page>>9)]);
 if(!pt||pt[page&511]!=(p|0x37)||!fwm_owned_page(&mem,p))return false;
 *perms=7;*hpa=p;*view=map(0,p);return *view!=NULL;
}

/* Rebuild L1's post-exit guest state in VMCS01. Host CRs must match the
 * already validated L1 mode for this Linux single-address-space transition. */
static bool nested_reflect_host(void){
 struct nested_l1_context *ctx=nested_current;
 uint64_t cr0,cr3,cr4;
 if(!nvmx_peek(&ctx->vmcs,0x6c00,&cr0)||!nvmx_peek(&ctx->vmcs,0x6c02,&cr3)||
    !nvmx_peek(&ctx->vmcs,0x6c04,&cr4)||cr0!=ctx->l1_mode->cr0||cr3!=ctx->l1_mode->cr3||cr4!=ctx->l1_mode->cr4)
  return false;
 const uint32_t from[]={0x4c00,0x6c10,0x6c12,0x6c06,0x6c08,0x6c0a,0x6c0c,0x6c0e,0x6c14,0x6c16};
 const uint32_t to[]=  {0x482a,0x6824,0x6826,0x680e,0x6810,0x6814,0x6816,0x6818,0x681c,0x681e};
 struct nested_vmcs_batch batch={0};
 for(unsigned i=0;i<sizeof from/sizeof *from;i++){
  uint64_t v;if(!nvmx_peek(&ctx->vmcs,from[i],&v)||
   (i&&((v>>47)!=0&&(v>>47)!=0x1ffff))||!nested_batch_add(&batch,to[i],v))return false;
 }
 for(unsigned i=0;i<8;i++){
  uint64_t sel=0;
  if(i!=6&&(!nvmx_peek(&ctx->vmcs,0xc00+2*(i==7?6:i),&sel)||(sel&7)))return false;
  if((i==1||i==7)&&!sel)return false;
  uint64_t ar=i==1?0xa09b:i==7?0x8b:sel?0xc093:0x10000;
  if(!nested_batch_add(&batch,0x800+2*i,sel)||!nested_batch_add(&batch,0x4814+2*i,ar)||
     !nested_batch_add(&batch,0x4800+2*i,i==7?103:UINT32_MAX))return false;
  if((i<4||i==6)&&!nested_batch_add(&batch,0x6806+2*i,0))return false;
 }
 return nested_batch_add(&batch,0x4810,0xffff)&&nested_batch_add(&batch,0x4812,0xffff)&&
        nested_batch_add(&batch,0x6820,2)&&nested_batch_add(&batch,0x681a,0x400)&&
        nested_batch_add(&batch,0x2802,0)&&nested_batch_add(&batch,0x4824,0)&&
        nested_batch_add(&batch,0x4826,0)&&nested_batch_add(&batch,0x6822,0)&&
        nested_batch_exec(&batch,true);
}

static bool nested_save_exit(void){
 struct nested_l1_context *ctx=nested_current;
 if(!nvmx_entry_commit(&ctx->vmcs))return false;
 struct nested_vmcs_batch batch={0};
 unsigned slots[LVP_MAX_FIELDS],guest_count=0;
 for(unsigned i=0;i<host.plan.count;i++){
  uint32_t field=host.plan.fields[i].encoding;
  if(((field>>10)&3)!=2||field==0x482e||field==0x2804||field==0x2806)continue;
  if(!nested_batch_add(&batch,field,0))return false;
  slots[guest_count++]=i;
 }
 const uint32_t exit_fields[]={0x4402,0x4404,0x4408,0x440c,0x440e};
 for(unsigned i=0;i<sizeof exit_fields/sizeof *exit_fields;i++){
  if(!nested_batch_add(&batch,exit_fields[i],0))return false;
 }
 if(!nested_batch_exec(&batch,false))return false;
 for(unsigned i=0;i<batch.count;i++){
  uint64_t value=batch.entry[i].value;
  if(!nvmx_exit_store(&ctx->vmcs,batch.entry[i].field,value))return false;
  if(i<guest_count)ctx->cpu.fields[slots[i]]=value;
 }
 uint64_t vectoring=batch.entry[guest_count+2].value,vector_error;
 if((vectoring&0x80000800u)==0x80000800u&&
    (!rd(0x440a,&vector_error)||!nvmx_exit_store(&ctx->vmcs,0x440a,vector_error)))return false;
 if(ctx->entry.reason==0){
  uint64_t info=batch.entry[guest_count+1].value,error;
  if((info&(1u<<11))&&(!rd(0x4406,&error)||!nvmx_exit_store(&ctx->vmcs,0x4406,error)))return false;
 }
 if(ctx->entry.reason==1){
  if(!nvmx_exit_store(&ctx->vmcs,0x4402,1)||
     !nvmx_exit_store(&ctx->vmcs,0x4404,0x80000000u|ctx->entry.interrupt_vector))return false;
 }
 if(ctx->entry.reason==49){
  if(!nvmx_exit_store(&ctx->vmcs,0x4402,49)||!nvmx_exit_store(&ctx->vmcs,0x2400,ctx->entry.gpa))return false;
 }
 if(ctx->entry.reason==48){
  uint64_t gla;
  if(!nvmx_exit_store(&ctx->vmcs,0x2400,ctx->entry.gpa))return false;
  if((ctx->entry.qualification&128)&&(!rd(0x640a,&gla)||!nvmx_exit_store(&ctx->vmcs,0x640a,gla)))return false;
 }
 return nvmx_exit_store(&ctx->vmcs,0x6400,ctx->entry.qualification)&&nvmx_return_from_l2(&ctx->vmcs);
}

/* Re-entry includes software exceptions interrupted during IDT delivery. */
static bool nested_event_valid(uint64_t info){
 if(!(info&(1u<<31)))return true;
 if(info&~UINT64_C(0x80000fff))return false;
 unsigned type=(info>>8)&7, vector=info&255;
 if(type==0)return !(info&(1u<<11));
 if(type==6)return (vector==3||vector==4)&&!(info&(1u<<11));
 if(type!=3||vector>=32)return false;
 const uint32_t error_vectors=0x60227d00u; /* vectors with error codes */
 return !!(info&(1u<<11))==!!(error_vectors&(1u<<vector));
}

static bool nested_reentry_event(struct nested_l1_context *ctx){
 uint64_t vectoring,err;
 if(!rd(0x4408,&vectoring))return false;
 ctx->entry.vectoring=vectoring;
 if(!(vectoring&(1u<<31)))vectoring=0;
 if(!nested_event_valid(vectoring))return false;
 if((vectoring&(1u<<11))&&(!rd(0x440a,&err)||!wr(0x4018,err)))return false;
 if((vectoring&0x80000700u)==0x80000600u){
  uint64_t len;
  if(!rd(0x440c,&len)||!len||len>15||!wr(0x401a,len))return false;
 }
 if(!wr(0x4016,vectoring))return false;
 return true;
}

#ifdef SYLE_TRACE_L2_CLOCK
/* The fixture sets R10/R11 explicitly before its userspace RDTSC. */
static struct nested_clock_trace {
 uint64_t index,owner,host_ref,host_exit_tsc,offset,guest_tsc;
 struct nested_timer_counters timer;
} nested_clock_trace[64];
static unsigned nested_clock_count;
static void nested_clock_report(void){
 for(unsigned i=0;i<nested_clock_count;i++){
  const struct nested_clock_trace *s=&nested_clock_trace[i];
  log_text("L2.CLOCK index=");log_hex(s->index);
  log_text(" owner=");log_hex(s->owner);
  log_text(" host_ref=");log_hex(s->host_ref);
  log_text(" host_exit_tsc=");log_hex(s->host_exit_tsc);
  log_text(" offset=");log_hex(s->offset);
  log_text(" guest_tsc=");log_hex(s->guest_tsc);log_text("\n");
  log_text("L2.TIMER index=");log_hex(s->index);
  log_text(" l1_expired=");log_hex(s->timer.l1_expired);
  log_text(" l1_late_ticks=");log_hex(s->timer.l1_late_ticks);
  log_text(" l1_max_late=");log_hex(s->timer.l1_max_late);
  log_text(" l1_batched=");log_hex(s->timer.l1_batched);
  log_text(" l1_accept=");log_hex(s->timer.l1_accept);
  log_text(" l1_reflect=");log_hex(s->timer.l1_reflect);
  log_text(" l2_submit=");log_hex(s->timer.l2_submit);
  log_text(" l2_deliver=");log_hex(s->timer.l2_deliver);
  log_text(" l1_vmx_expired=");log_hex(s->timer.l1_vmx_expired);log_text("\n");
 }
}
#endif
static bool nested_rdtsc(void){
 struct nested_l1_context *ctx=nested_current;
 uint64_t len,rip,flags,cs,cr4,primary,offset=0;
 struct lhc_sample sample;
 if(!nvmx_peek(&ctx->vmcs,0x4002,&primary)||
    !rd(0x440c,&len)||len!=2||!rd(0x681e,&rip)||!rd(0x6820,&flags)||
    !rd(0x0802,&cs)||!rd(0x6804,&cr4)||((cs&3)&&(cr4&4))||
    !tsc_source_ready()||!clock_source.sample(clock_source.ctx,&sample)||
    !ftsc_sample(&virtual_tsc,host.vmcs_page,sample.epoch,sample.cpu,sample.tsc))return false;
 if((primary&(1u<<3))&&!nvmx_peek(&ctx->vmcs,0x2010,&offset))return false;
 uint64_t value=sample.tsc-virtual_tsc.origin+offset;
#ifdef SYLE_TRACE_L2_CLOCK
 uint64_t tag,index;
 if((cs&3)==3&&vcpu_reg_read(ctx->l1_native->gpr,10,&tag)&&
    tag==UINT64_C(0x53594c45434c4b31)&&vcpu_reg_read(ctx->l1_native->gpr,11,&index)&&
    nested_clock_count<sizeof nested_clock_trace/sizeof *nested_clock_trace)
  nested_clock_trace[nested_clock_count++]=(struct nested_clock_trace){
   index,ctx-nested_contexts,sample.tsc,ctx->l1_native->exit_tsc,offset,value,
   nested_timer_counters[ctx-nested_contexts]};
#endif
 if(!wr(0x681e,rip+len)||!wr(0x6820,flags&~UINT64_C(0x10000)))return false;
 ctx->l1_native->gpr[0]=(uint32_t)value;ctx->l1_native->gpr[3]=value>>32;
 return true;
}

/* The L2 i8259 masks IRQ0, issues specific EOI 0x60 and unmasks IRQ0
 * before returning. Measure only complete, uninterrupted sequences. */
static void nested_irq0_report(void);
static bool nested_execute_slice(void);
#ifdef SYLE_M2_DUAL_VCPU
static bool nested_preempt_ticks(const struct nested_l1_context *ctx,uint64_t *ticks){
 uint64_t quantum=UINT64_C(1)<<16;
#ifdef SYLE_NESTED_VMX_TIMER
 if(ctx->entry.timer_enabled){
  struct lhc_sample sample;
  if(!clock_source.sample(clock_source.ctx,&sample))return false;
  if(sample.tsc>=ctx->entry.timer_deadline){*ticks=0;return true;}
  /* Fewer L0 round trips when KVM's deadline is far away. Keep the
   * short slice near the deadline and when using L1 software timers. */
  if(ctx->entry.timer_deadline-sample.tsc>=2000)quantum<<=3;
 }
#else
 (void)ctx;
#endif
 *ticks=quantum>>host.caps.timer_shift;
 if(!*ticks)*ticks=1;
 return true;
}
#endif
static bool nested_run_entry(void){
 struct nested_l1_context *ctx=nested_current;
 ctx->entry=(__typeof__(ctx->entry)){0};
 if(!nvmx_peek(&ctx->vmcs,0x6800,&ctx->entry.guest_cr0)||
    !nvmx_peek(&ctx->vmcs,0x6802,&ctx->entry.guest_cr3)||
    !nvmx_peek(&ctx->vmcs,0x6804,&ctx->entry.guest_cr4)||
    !nvmx_peek(&ctx->vmcs,0x201a,&ctx->entry.eptp12)||
    !nvmx_peek(&ctx->vmcs,0x4014,&ctx->entry.entry_count)||
    !nvmx_peek(&ctx->vmcs,0x4010,&ctx->entry.exit_count)||
    !nvmx_peek(&ctx->vmcs,0x400e,&ctx->entry.store_count))return false;
 uint64_t en,secondary,event,flags,rip,link;
 if(!nvmx_peek(&ctx->vmcs,0x4012,&en)||!nvmx_peek(&ctx->vmcs,0x401e,&secondary)||
    !nvmx_peek(&ctx->vmcs,0x4016,&event)||!nvmx_peek(&ctx->vmcs,0x6820,&flags)||
    !nvmx_peek(&ctx->vmcs,0x681e,&rip)||!nvmx_peek(&ctx->vmcs,0x2800,&link))return false;
 ctx->entry.en=en;ctx->entry.sec=secondary;ctx->entry.event=event;ctx->entry.rip=rip;ctx->entry.link=link;
 ctx->entry.stage=1;
 if((event&0x800007ffu)==0x80000030u){
  ctx->irq0_cost.pending=true;ctx->irq0_cost.eoi=false;
  ctx->irq0_cost.begin=active_apic.reference;ctx->irq0_cost.ops_begin=ctx->irq0_cost.vmx_ops;
 }
 /* The virtual controls omit LOAD_EFER. For IA-32e entry, inherit
  * L1's guest EFER and set LME/LMA in the L0-owned VMCS02. */
 const uint32_t ia32e_entry=1u<<9;
 bool long_mode=(en&ia32e_entry)!=0;
 bool unrestricted=false;
#ifdef SYLE_M2_DUAL_VCPU
 unrestricted=(secondary&128)&&(host.caps.secondary&(UINT64_C(1)<<39));
#endif
 uint64_t cs_ar,dr7,debugctl;
 if(!nvmx_peek(&ctx->vmcs,0x4816,&cs_ar)||!nvmx_peek(&ctx->vmcs,0x681a,&dr7)||
    !nvmx_peek(&ctx->vmcs,0x2802,&debugctl)||debugctl||(dr7&~UINT64_C(0x400)))return false;
 if(long_mode){
  uint64_t code=cs_ar&0x16098;
  /* IA-32e is enabled before the AP's far jump to a 64-bit CS. */
  bool compatibility=unrestricted&&(code==0x98||code==0x4098)&&rip<=UINT32_MAX;
  if((code!=0x2098&&!compatibility)||(flags&(1u<<17)))return false;
 }
 /* Long-mode fetches remain constrained by guest paging and EPT02. */
 if(long_mode){
  if(((rip>>47)!=0&&(rip>>47)!=0x1ffff)||
     (ctx->entry.guest_cr0&~UINT64_C(0x5000e))!=0x80000031||
     (ctx->entry.guest_cr4&~UINT64_C(0x6d8))!=0x2020)return false;
 }else if(unrestricted){
  /* An AP enters at its SIPI vector and may run 16-bit or protected-mode
   * trampoline code before switching to IA-32e. EPT02 still owns every
   * instruction fetch and guest page-table walk. */
  if(rip>UINT32_MAX||(ctx->entry.guest_cr0&~UINT64_C(0xe005003f))||
     (ctx->entry.guest_cr4&~UINT64_C(0x26f8))||!(ctx->entry.guest_cr4&0x2000))return false;
 }else if(rip>=4096||ctx->entry.guest_cr0!=0x80000031||ctx->entry.guest_cr4!=0x2011)return false;
 if((en&~(4u|ia32e_entry))||secondary!=(unrestricted?130u:2u)||!nested_event_valid(event)||link!=UINT64_MAX||
    (ctx->entry.guest_cr3&~UINT64_C(0xfffff000))||
    (ctx->entry.eptp12&4095)!=0x1e||(ctx->entry.eptp12>>32)||
    ctx->entry.entry_count||ctx->entry.exit_count||ctx->entry.store_count)return false;
 static const uint32_t host_fields[]={
  0xc00,0xc02,0xc04,0xc06,0xc08,0xc0a,0xc0c,0x4c00,0x2c00,0x2c02,
  0x6c00,0x6c02,0x6c04,0x6c06,0x6c08,0x6c0a,0x6c0c,0x6c0e,0x6c10,0x6c12,0x6c14,0x6c16};
 uint64_t host_values[sizeof host_fields/sizeof *host_fields];
 bool fresh=!ctx->cpu.page,loaded=false;
 ctx->entry.stage=2;
 if(ctx->composed_source!=ctx->entry.eptp12)nested_drop_ept();
 if(fresh){
  for(unsigned i=0;i<sizeof host_fields/sizeof *host_fields;i++)
   if(!rd(host_fields[i],&host_values[i]))return false;
  uint64_t page=alloc(0);
  if(!page)return false;
  if(page==host.vmcs_page||page==host.vmxon_page||fwm_owned_page(&mem,page))fatal();
  uint32_t *region=map(0,page);
  if(!region||!wb(0,page)){if(!release(0,page))fatal();return false;}
  for(unsigned i=0;i<1024;i++)region[i]=0;
  region[0]=host.basic&0x7fffffff;
  if(!nested_clear(page))fatal();
  ctx->cpu.page=ctx->composed_budget.vmcs=page;
 }
 const struct nested_sparse_ops sparse_ops={nested_page_alloc,nested_page_free,nested_flush};
 if(!ctx->composed.root&&!nested_ept_sparse_init(&ctx->composed,&sparse_ops,&ctx->composed_budget,ops.physical_bits))goto done;
 ctx->composed_source=ctx->entry.eptp12;
 if(!FW_SWITCH(*ctx->l1_page,ctx->cpu.page))goto done;
 loaded=true;ctx->entry.stage=3;
 if(fresh)for(unsigned i=0;i<sizeof host_fields/sizeof *host_fields;i++)
  if(!wr(host_fields[i],host_values[i]))goto done;
 struct nested_vmcs_batch batch={0};
 for(unsigned i=0;i<host.plan.count;i++){
  uint32_t f=host.plan.fields[i].encoding;
  ctx->entry.field=f;
  uint64_t v=host.plan.fields[i].value;
  /* Guest-state fields only. Physical controls always come from the L0 plan. */
  if(((f>>10)&3)==2 && f!=0x482e && f!=0x2804 && f!=0x2806){
   if(!nvmx_peek(&ctx->vmcs,f,&v))goto done;
   if(f==0x6804)v|=0x2000;
  }
  /* Page-table roots resolve through EPT02. Honor KVM's CR3, INVLPG,
   * interrupt-window and PAUSE controls; retain L0's other intercepts. */
  if((long_mode||unrestricted)&&f==0x4002){
   uint64_t primary;
   if(!nvmx_peek(&ctx->vmcs,f,&primary))goto done;
   const uint32_t selective_exits=(1u<<2)|(1u<<9)|(1u<<15)|(1u<<16)|(1u<<30);
   v=(v&~selective_exits)|(primary&selective_exits);
  }
  if((long_mode||unrestricted)&&(f==0x6000||f==0x6002||f==0x6004||f==0x6006))
   if(!nvmx_peek(&ctx->vmcs,f,&v))goto done;
  if(long_mode&&f==0x4012)v|=ia32e_entry;
  if(long_mode&&f==0x2806)v=ctx->l1_mode->efer|0x500;
  if(f==0x4016)v=(event&(1u<<31))?event:0;
  if(f==0x401a&&(event&0x80000700u)==0x80000600u&&
     (!nvmx_peek(&ctx->vmcs,f,&v)||!v||v>15))goto done;
  if(f==0x4018&&(event&(1u<<11))&&!nvmx_peek(&ctx->vmcs,f,&v))goto done;
  if(f==0x201a)v=ctx->composed.eptp;
  if(f==0x4004)v=UINT32_MAX;
  if(long_mode&&(f==0x4004||f==0x4006||f==0x4008))
   if(!nvmx_peek(&ctx->vmcs,f,&v))goto done;
  /* Event delivery, the timer and hardware-saved MSRs may have changed
   * outside the reflected VMCS12 state. Always refresh those fields. */
  if(fresh||ctx->cpu.fields[i]!=v||f==0x482e||f==0x2804||f==0x2806||
     f==0x4016||f==0x4018||f==0x401a){
   if(!nested_batch_add(&batch,f,v))goto done;
   ctx->cpu.fields[i]=v;
  }
 }
 if(!nested_batch_exec(&batch,true))goto done;
 ctx->entry.stage=4;
 ctx->entry.long_mode=long_mode;
 ctx->entry.unrestricted=unrestricted;
#ifdef SYLE_NESTED_VMX_TIMER
 uint64_t pin,exit;
 if(!nvmx_peek(&ctx->vmcs,0x4000,&pin)||!nvmx_peek(&ctx->vmcs,0x400c,&exit)||
    (exit&(1u<<22)))goto done; /* VM-exit timer save is not advertised. */
 if(pin&(1u<<6)){
  uint64_t count;
  struct lhc_sample sample;
  if(!nvmx_peek(&ctx->vmcs,0x482e,&count)||!clock_source.sample(clock_source.ctx,&sample)||
     sample.tsc>UINT64_MAX-(count<<NESTED_TIMER_SHIFT))goto done;
  ctx->entry.timer_deadline=sample.tsc+(count<<NESTED_TIMER_SHIFT);
  ctx->entry.timer_enabled=true;
 }
#endif
#ifdef SYLE_TRACE_L2_CLOCK
 if((event&0x800007ffu)==0x800000ecu){
  ctx->entry.trace_timer_pending=true;
  nested_timer_counters[ctx-nested_contexts].l2_submit++;
 }
#endif
 return nested_execute_slice();
done:
 if(loaded&&!FW_SWITCH(ctx->cpu.page,*ctx->l1_page))fatal();
 nested_drop_cpu();
 nested_drop_ept();
 return false;
}

static bool nested_execute_slice(void){
 struct nested_l1_context *ctx=nested_current;
 bool long_mode=ctx->entry.long_mode,unrestricted=ctx->entry.unrestricted,reflect=false;
 ctx->entry.suspended=false;
 uint32_t previous_resume=ctx->l1_native->resume;
 unsigned attempt=0,limit=long_mode?32768u:32u;
#ifdef SYLE_M2_DUAL_VCPU
 if(limit>256)limit=256;
#endif
 for(;attempt<limit;attempt++){
  ctx->entry.attempt=attempt;
  enum lvs_result outcome;
#ifdef SYLE_M2_HOST_PARALLEL
  if(ctx->entry.prefetched){
   outcome=ctx->entry.outcome;
   ctx->entry.prefetched=false;
  }else
#endif
  {
#ifdef SYLE_M2_DUAL_VCPU
   uint64_t ticks;
   if(!nested_preempt_ticks(ctx,&ticks)||!wr(0x482e,ticks))break;
#endif
   ctx->l1_native->resume=ctx->cpu.launched;
#ifdef SYLE_M2_HOST_PARALLEL
   outcome=fmp_nested_run(ctx,attempt==0);
#else
   outcome=lvs_run(ctx->l1_state,ctx->l1_ops);
#endif
   ctx->l1_native->resume=previous_resume;
  }
  if(!safe())fatal();
  if(outcome!=LVS_EXIT||ctx->l1_native->entry_status!=1){
   rd(0x4400,&ctx->entry.vm_instruction_error);break;
  }
  ctx->cpu.launched=true;
  if(!rd(0x4402,&ctx->entry.reason)||!rd(0x6400,&ctx->entry.qualification))break;
  ctx->entry.entered=!(ctx->entry.reason>>31);
  if(!ctx->entry.entered)break;
#ifdef SYLE_TRACE_L2_CLOCK
  if(ctx->entry.trace_timer_pending){
   uint64_t vectoring;
   if(!rd(0x4408,&vectoring))break;
   if((vectoring&0x800007ffu)!=0x800000ecu){
    nested_timer_counters[ctx-nested_contexts].l2_deliver++;
    ctx->entry.trace_timer_pending=false;
   }
  }
#endif
  if(ctx->entry.reason==29){
   uint64_t primary,regs[16],ip,fl,sz,cs;
   if(!nvmx_peek(&ctx->vmcs,0x4002,&primary))break;
   if(primary&(1u<<23)){reflect=nested_save_exit();break;}
   if(!rd(0x0802,&cs)||(cs&3)||!rd(0x681e,&ip)||!rd(0x6820,&fl)||!rd(0x440c,&sz))break;
   for(unsigned i=0;i<16;i++)if(!vcpu_reg_read(ctx->l1_native->gpr,i,&regs[i]))goto done;
   if(!fbd_apply_qualified(ctx->debug,regs,ip,fl,ctx->entry.qualification,sz,0,irq_write))break;
   unsigned reg=(ctx->entry.qualification>>8)&15;
   if((ctx->entry.qualification&16)&&reg!=4&&!vcpu_reg_write(ctx->l1_native->gpr,reg,regs[reg]))break;
   if(!wr(0x4016,0))break;
   continue;
  }
  if(ctx->entry.reason==16){
   uint64_t primary;
   if(!nvmx_peek(&ctx->vmcs,0x4002,&primary))break;
   if(primary&(1u<<12)){reflect=nested_save_exit();break;}
   if(!nested_rdtsc()||!wr(0x4016,0))break;
   continue;
  }
  if(ctx->entry.reason==54&&unrestricted&&!long_mode){
   uint64_t ip,len;
   bool attempted=false;
   if(ctx->entry.qualification||!rd(0x681e,&ip)||!rd(0x440c,&len)||len!=2||
      !cache_maintain(&attempted)||!wr(0x681e,ip+len)||
      !wr(0x4016,0))break;
   continue;
  }
  if(ctx->entry.reason==52){
#ifdef SYLE_M2_DUAL_VCPU
#ifdef SYLE_NESTED_VMX_TIMER
   if(ctx->entry.timer_enabled){
    struct lhc_sample sample;
    if(!clock_source.sample(clock_source.ctx,&sample))break;
    if(sample.tsc>=ctx->entry.timer_deadline){
     reflect=nested_save_exit();
#ifdef SYLE_TRACE_L2_CLOCK
     if(reflect)nested_timer_counters[ctx-nested_contexts].l1_vmx_expired++;
#endif
     break;
    }
   }
#endif
   if(!nested_reentry_event(ctx))break;
   attempt=limit;
   break;
#else
   break;
#endif
  }
  if(ctx->entry.reason!=48){
   if(ctx->entry.reason==7||ctx->entry.reason==30||ctx->entry.reason==12||ctx->entry.reason==10||
      ctx->entry.reason==14||ctx->entry.reason==28||ctx->entry.reason==31||ctx->entry.reason==32)reflect=nested_save_exit();
   if(ctx->entry.reason==0){
    uint64_t info,bitmap;
    if(rd(0x4404,&info)&&(info&0x800007ffu)==0x8000030du&&
       nvmx_peek(&ctx->vmcs,0x4004,&bitmap)&&(bitmap&(1u<<13)))reflect=nested_save_exit();
   }
   break;
  }
  if(!rd(0x2400,&ctx->entry.gpa))break;
  struct nested_ept_policy policy={32,false,false};
  uint64_t base=ctx->entry.gpa&~UINT64_C(4095);
  struct nested_ept_result resolved=nested_ept_walk_memory_classified(ctx->entry.eptp12,base,
    1,nested_ram_page,0,&policy);
  resolved=nested_sparse_walk_access(resolved,ctx->entry.qualification&7);
  ctx->entry.walk_status=resolved.status;
  if(resolved.status==NESTED_EPT_MISCONFIG){
   /* KVM encodes MMIO in deliberately misconfigured EPT12 entries. */
   ctx->entry.reason=49;ctx->entry.qualification=0;
   reflect=nested_save_exit();break;
  }
  if(resolved.status==NESTED_EPT_L1_DENIED){
   ctx->entry.qualification=(ctx->entry.qualification&0x187)|((uint64_t)resolved.effective12<<3);
   reflect=nested_save_exit();break;
  }
  if(resolved.status!=NESTED_EPT_OK)break;
  /* Keep the composed EPT bounded as Linux touches fresh pages. */
  if(ctx->composed_budget.pages>502&&!nested_ept_sparse_reset(&ctx->composed))break;
  ctx->entry.fill_status=nested_ept_sparse_fill(&ctx->composed,base,&resolved);
  if(ctx->entry.fill_status==NESTED_SPARSE_EXISTS){
   /* L1 can relax a cached read-only SPTE without an explicit flush. */
   if(!nested_ept_sparse_reset(&ctx->composed))break;
   ctx->entry.fill_status=nested_ept_sparse_fill(&ctx->composed,base,&resolved);
  }
  if(ctx->entry.fill_status!=NESTED_SPARSE_OK)break;
  if(!nested_reentry_event(ctx))break;
 }
#ifdef SYLE_M2_DUAL_VCPU
 if(attempt==limit){
   uint64_t pin,exit;
   uint8_t vector;
   if(nvmx_peek(&ctx->vmcs,0x4000,&pin)&&(pin&1)&&
      nvmx_peek(&ctx->vmcs,0x400c,&exit)&&(exit&(1u<<15))&&
      nested_l1_irq_poll(&vector)){
    /* ACK-on-exit consumes the selected L1 LAPIC interrupt. Report its
     * real vector to KVM after committing the saved L2 state. */
    ctx->entry.reason=1;
    ctx->entry.interrupt_vector=vector;
    reflect=nested_save_exit()&&nested_l1_irq_accept(vector);
    goto done;
   }
   /* L0's timer expires while L2 owns the register bank. Keep VMCS02
    * parked and switch the other L1 CPU in. The virtual KVM entry only
    * completes when a real L2 exit is reflected to the owning L1 CPU. */
   ctx->entry.suspended=true;
   if(!FW_SWITCH(ctx->cpu.page,*ctx->l1_page))fatal();
   return true;
 }
#endif
done:
 if(reflect&&ctx->irq0_cost.pending&&ctx->entry.reason==30){
  uint64_t q=ctx->entry.qualification;
  if((q&0xffff003f)==0x00200000&&(uint8_t)ctx->l1_native->gpr[0]==0x60)ctx->irq0_cost.eoi=true;
  if(ctx->irq0_cost.eoi&&(q&0xffff003f)==0x00210000&&!(ctx->l1_native->gpr[0]&1)){
   uint64_t ticks=active_apic.reference-ctx->irq0_cost.begin;
   ctx->irq0_cost.pending=false;ctx->irq0_cost.elapsed+=ticks;ctx->irq0_cost.samples++;
   ctx->irq0_cost.ops_total+=ctx->irq0_cost.vmx_ops-ctx->irq0_cost.ops_begin;
   if(ticks>=40000)ctx->irq0_cost.over_period++;
   if(ctx->irq0_cost.samples==256||ctx->irq0_cost.samples==1024)nested_irq0_report();
  }
 }
 if(!reflect)rd(0x681e,&ctx->entry.exit_rip);
 if(!FW_SWITCH(ctx->cpu.page,*ctx->l1_page))fatal();
 if(reflect)reflect=nested_reflect_host();
 if(!reflect){nested_drop_cpu();nested_drop_ept();}
 return reflect;
}

#ifdef SYLE_M2_DUAL_VCPU
static bool nested_resume_slice(void){
 struct nested_l1_context *ctx=nested_current;
 if(!ctx->entry.suspended||!FW_SWITCH(*ctx->l1_page,ctx->cpu.page))return false;
 return nested_execute_slice();
}
#endif
