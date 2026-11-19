/* Two-vCPU physical xAPIC startup. Only the BSP runs while its ICR MOV
 * is decoded and committed. AP execution has separate registers and VMCS. */
static bool firmware_device_mov(const struct fmm_memory *m,const struct fat_exit *e,
                              const uint64_t *g,struct guest_mmio_mov *mov,uint64_t base){
 static const unsigned indices[]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
 if(!firmware_ap.page||e->ds_base||e->es_base||e->ss_base||e->entry_event||
    e->vectoring||e->blocking||e->pending_debug||e->dr7!=0x400)return false;
 uint64_t regs[16];
 for(unsigned i=0;i<16;i++)regs[i]=i==4?e->rsp:g[indices[i]];
 uint8_t bytes[15];
 size_t n=fmm_fetch(m,e->rip,bytes);
 return guest_mmio_mov_decode(bytes,n,e->rip,regs,mov)&&mov->width==4&&
        fmm_validate(m,mov->linear,4,mov->write,e->reason,e->qualification,e->linear,
                     e->physical,e->vectoring,base,4096);
}
static bool firmware_apic_mov(const struct fmm_memory *m,const struct fat_exit *e,
                              const uint64_t *g,struct guest_mmio_mov *mov){
 return firmware_device_mov(m,e,g,mov,0xfee00000);
}
/* Free-running HPET clocksource. Three 32-bit channels have no IRQ routes,
 * periodic mode, FSB delivery or legacy replacement capability. */
static bool linux_hpet(const struct fmm_memory *m,const struct fat_exit *e,uint64_t g[15]){
 static const unsigned indices[]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
 struct guest_mmio_mov mov;
 if(!firmware_device_mov(m,e,g,&mov,0xfed00000))return false;
 struct lhc_sample sample;
 if(!clock_source.sample(clock_source.ctx,&sample)||sample.epoch!=active_apic.epoch||
    sample.cpu!=clock_source.properties.cpu||sample.tsc<active_apic.reference||
    sample.tsc-active_apic.reference>clock_source.max_gap)return false;
 uint32_t count=firmware_ap.hpet_count;
 if(firmware_ap.hpet_enabled)count+=(uint32_t)(sample.tsc-firmware_ap.hpet_started);
 unsigned off=e->physical-0xfed00000;
 uint32_t v=mov.store_value;
 switch(off){
 case 0: if(mov.write)return false;v=0x80860201;break;
 case 4: if(mov.write)return false;v=100000000;break; /* femtoseconds per tick */
 case 0x10:
  if(mov.write){
   if(v&~1u)return false;
   firmware_ap.hpet_count=count;firmware_ap.hpet_started=sample.tsc;firmware_ap.hpet_enabled=v&1;
  }else v=firmware_ap.hpet_enabled;
  break;
 case 0xf0:
  if(mov.write){
   if(firmware_ap.hpet_enabled)return false;
   firmware_ap.hpet_count=v;
  }else v=count;
  break;
 case 0x108:case 0x128:case 0x148:
  if(mov.write)firmware_ap.hpet_compare[(off-0x108)/32]=v;
  else v=firmware_ap.hpet_compare[(off-0x108)/32];
  break;
 case 0x14:case 0x20:case 0x24:case 0xf4:
 case 0x100:case 0x120:case 0x140:
 case 0x104:case 0x124:case 0x144:
 case 0x10c:case 0x12c:case 0x14c:
  if(mov.write&&v)return false;
  v=0;break;
 default:return false;
 }
 if(!mov.write){
  if(mov.reg==4){if(!wr(0x681c,v))return false;}
  else g[indices[mov.reg]]=v;
 }
 return (!(e->rflags&0x10000)||wr(0x6820,e->rflags&~UINT64_C(0x10000)))&&wr(0x681e,mov.next_rip);
}
static bool linux_imcr_io(uint64_t g[15],uint64_t rip,uint64_t q,unsigned len){
 uint8_t bytes[15];size_t n=io_instruction_fetch(rip,bytes);
 unsigned port=q>>16;
 if((q&7)||!fpp_encoding(q,len,bytes,n)||(!(q&64)&&(uint16_t)g[3]!=port))return false;
 if(port==0x22&&(q&8))return false;
 if(port==0x23&&firmware_ap.imcr_index!=0x70)return false;
 if(!wr(0x681e,rip+len))return false;
 if(port==0x22)firmware_ap.imcr_index=g[0];
 else if(q&8)g[0]=(g[0]&~UINT64_C(255))|firmware_ap.imcr;
 else firmware_ap.imcr=g[0]&1;
 return true;
}
static bool linux_lapic_poll(struct guest_apic *apic,uint64_t *now){
 if(active_apic.reference<active_apic.clock.timer.origin)return false;
 uint64_t elapsed=active_apic.reference-active_apic.clock.timer.origin;
 if(elapsed>UINT64_MAX/(10*LAPIC_TIMER_BASE_TICKS))return false;
 *now=elapsed*10*LAPIC_TIMER_BASE_TICKS;
 /* A descheduled vCPU can miss several periodic deadlines, or a new
  * period can expire with its vector already in IRR. Keep the excess in
  * the timer device and offer one edge whenever IRR has room again. */
 uint64_t *backlog=&firmware_ap.timer_backlog[apic==&firmware_ap.apic];
 unsigned vector=apic->timer.lvt&255;
 uint32_t bit=UINT32_C(1)<<(vector%32);
 bool pending=(apic->irq.irr[vector/32]&bit)!=0;
#ifdef SYLE_TRACE_L2_CLOCK
 const uint64_t deadline=apic->timer.deadline;
#endif
 if(!guest_apic_poll(apic,*now))return false;
#ifdef SYLE_TRACE_L2_CLOCK
 if(vector==0xec&&!(apic->timer.lvt&(1u<<16))&&apic->timer.last_expirations){
  struct nested_timer_counters *t=&nested_timer_counters[apic==&firmware_ap.apic];
  uint64_t late=(*now-deadline)/(10*LAPIC_TIMER_BASE_TICKS);
  t->l1_expired+=apic->timer.last_expirations;
  t->l1_batched+=apic->timer.last_expirations-1;
  t->l1_late_ticks+=late;
  if(late>t->l1_max_late)t->l1_max_late=late;
 }
#endif
 if(!(apic->irq.svr&0x100)||(apic->timer.lvt&0x30000)!=0x20000){
  *backlog=0;
  return true;
 }
 uint64_t expired=apic->timer.last_expirations;
 if(expired){
  uint64_t excess=expired-!pending;
  if(excess>UINT64_MAX-*backlog)return false;
  *backlog+=excess;
 }else if(*backlog&&!pending){
  if(!guest_apic_enqueue(apic,vector,false))return false;
  --*backlog;
 }
 return true;
}
#ifdef SYLE_M2_NESTED
static bool nested_l1_irq_poll(uint8_t *vector){
 struct guest_apic *apic=nested_current==nested_contexts?&firmware_ap.bsp_linux:&firmware_ap.apic;
 uint64_t now;
 return linux_lapic_poll(apic,&now)&&guest_apic_select(apic,vector);
}
static bool nested_l1_irq_accept(uint8_t vector){
 struct guest_apic *apic=nested_current==nested_contexts?&firmware_ap.bsp_linux:&firmware_ap.apic;
 if(!guest_apic_accept(apic,vector))return false;
#ifdef SYLE_TRACE_L2_CLOCK
 if(vector==0xec)nested_timer_counters[nested_current-nested_contexts].l1_reflect++;
#endif
 return true;
}
#endif
static bool linux_lapic_before(struct guest_apic *apic,unsigned *injected){
 uint8_t vector;uint64_t flags,block,event,now;
 if(!linux_lapic_poll(apic,&now))return false;
 if(!guest_apic_select(apic,&vector))return true;
 if(!rd(0x6820,&flags)||!rd(0x4824,&block)||!rd(0x4016,&event))return false;
 if(!(flags&512)||(block&3)||event)return true;
 if(!wr(0x4016,0x80000000u|vector))return false;
 *injected=vector;return true;
}
static bool linux_lapic_after(struct guest_apic *apic,unsigned vector,uint64_t reason,uint64_t vectoring){
 if((reason>>16)||(vectoring&0x80000000)||!guest_apic_accept(apic,vector)||!wr(0x4016,0))return false;
#ifdef SYLE_TRACE_L2_CLOCK
 if(vector==0xec)nested_timer_counters[apic==&firmware_ap.apic].l1_accept++;
#endif
 return true;
}
static bool linux_lapic_register(struct guest_apic *apic,uint32_t off,bool write,uint32_t *value){
 uint64_t now;if(!linux_lapic_poll(apic,&now))return false;
 uint32_t old_lvt=apic->timer.lvt;
 struct guest_apic_effect effect={0};
 if(write&&!(apic->irq.svr&0x100)&&(off==0x2f0||(off>=0x320&&off<=0x370)))
  *value|=1u<<16;
 if(write&&off==0xf0){
  if(*value&~0x3ffu)return false;
  if(!(*value&0x100)){
   for(unsigned i=0;i<6;i++)apic->lvt[i]|=1u<<16;
   apic->timer.lvt|=1u<<16;
  }
  apic->irq.svr=*value|15;
  if(apic==&firmware_ap.bsp_linux)firmware_ap.svr_changes++;
 }else if(write&&(off==0x350||off==0x360||off==0x370)&&!(*value&~(off==0x370?0x100ffu:0x1a7ffu))){
  apic->lvt[off==0x350?3:off==0x360?4:5]=*value;
 }else if(write&&off==0x3e0&&*value==apic->timer.divide){
  /* Re-selecting the current divisor preserves the running countdown. */
 }else if(!guest_apic_mmio(apic,off,4,write,value,now,&effect))return false;
 if(write&&(off==0x380||apic->timer.lvt!=old_lvt))
  firmware_ap.timer_backlog[apic==&firmware_ap.apic]=0;
 return true;
}
static bool linux_bsp_mmio(const struct fmm_memory *m,const struct fat_exit *e){
 struct guest_mmio_mov mov;
 if(!firmware_apic_mov(m,e,native.gpr,&mov))return false;
 uint32_t off=e->physical-0xfee00000,value=mov.store_value;
 if(!firmware_ap.handoff){
  if(!mov.write)return fiap_mmio(&active_apic.device,&irq_session,native.gpr,m,e,0,pic_write)==FAT_COMMITTED;
  if(off!=0xf0||(value&0x100)||!irq_session.ready||!irq_session.owner.masked||
     irq_session.owner.pending||irq_session.owner.in_service||irq_session.owner.reserved||
     irq_session.active_attempt||disk_pic_run.inflight||disk_pic_window||irq_window.armed)return false;
  /* The legacy source retains the firmware countdown separately from
   * the MMIO register bank. Carry its masked periodic phase into Linux. */
  const struct fti_owner *owner=&irq_session.owner;
  if(owner->origin!=active_apic.clock.timer.origin||owner->deadline_bus>UINT64_MAX/LAPIC_TIMER_BASE_TICKS)return false;
  firmware_ap.bsp_linux=active_apic.device.apic;
  struct lapic_timer *timer=&firmware_ap.bsp_linux.timer;
  timer->lvt=(3u<<16)|FTI_VECTOR;
  timer->divide=0xb;
  timer->initial=FTI_PERIOD_BUS;
  timer->period=FTI_PERIOD_BUS*LAPIC_TIMER_BASE_TICKS;
  timer->deadline=owner->deadline_bus*LAPIC_TIMER_BASE_TICKS;
  timer->armed=true;
  firmware_ap.handoff=true;
 }
 struct guest_apic *apic=&firmware_ap.bsp_linux;
 if(!linux_lapic_register(apic,off,mov.write,&value))return false;
 if(!mov.write&&!reg_write(mov.reg,value))return false;
 return (!(e->rflags&0x10000)||wr(0x6820,e->rflags&~UINT64_C(0x10000)))&&wr(0x681e,mov.next_rip);
}
static bool firmware_ap_init(void){
 if(!FW_SWITCH(host.vmcs_page,firmware_ap.page))return false;
 /* INIT preserves the FP image, MSR bank, cache control bits and PAT. */
 uint64_t cache_bits=firmware_ap.mode.cr0&UINT64_C(0x60000000),pat;
 struct fcm_state reset={.cr0=cache_bits|16};
 bool ok=rd(0x2804,&pat)&&
#ifdef SYLE_M2_HOST_PARALLEL
         fmp_call(FMP_RESET,0,0)&&
#else
         fwh_program_reset(&host,(uintptr_t)(fallback_stack+sizeof fallback_stack),(uintptr_t)fallback)&&
#endif
         wr(0x2804,pat)&&
         commit_vcpu_mode(&firmware_ap.mode,&reset)&&(!firmware_ap.handoff||wr(0x4004,0x42));
 if(!FW_SWITCH(firmware_ap.page,host.vmcs_page))fatal();
 if(!ok)return false;
 for(unsigned i=0;i<15;i++)firmware_ap.native.gpr[i]=host.plan.gpr[i];
 firmware_ap.native.cr2=0;
 firmware_ap.debug=(struct fbd_bank){0};
 firmware_ap.timer_backlog[1]=0;
 guest_apic_init_mode(&firmware_ap.apic,0,false);
 firmware_ap.timer=(struct ft100_mmio){0};
 if(!ft100_mmio_init(&firmware_ap.timer,active_apic.epoch,active_apic.reference,clock_source.max_gap))return false;
 firmware_ap.initialized=true;
 firmware_ap.runnable=false;
 firmware_ap.starts++;
 return true;
}
/* Physical and flat-logical fixed IPIs share the same two LAPICs as
 * timer interrupts. Enqueue on the target before it next gets a run slot. */
static bool linux_fixed_ipi(unsigned sender,uint32_t high,uint32_t command){
 if(command&~UINT32_C(0xc48ff)||(command&255)<32)return false;
 unsigned shorthand=(command>>18)&3,dest=high>>24;
 for(unsigned id=0;id<2;id++){
  struct guest_apic *target=id?&firmware_ap.apic:&firmware_ap.bsp_linux;
  bool explicit_target=(command&0x800)?((target->ldr>>24)&dest)!=0:(dest==id||dest==255);
  if(!(shorthand==1?id==sender:shorthand==2?true:shorthand==3?id!=sender:explicit_target))continue;
  if(!guest_apic_enqueue(target,command&255,false))return false;
#ifdef SYLE_M2_HOST_PARALLEL
  if(id!=sender)fmp.cross_ipis++;
#endif
 }
 return true;
}
static bool firmware_icr(const struct fmm_memory *m,const struct fat_exit *e){
 static const unsigned indices[]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
 struct guest_mmio_mov mov;
 if(!firmware_apic_mov(m,e,native.gpr,&mov))return false;
 bool high=e->physical==0xfee00310;
 uint32_t value=mov.store_value;
 if(mov.write&&!high){
  unsigned shorthand=(value>>18)&3,delivery=(value>>8)&7;
  bool target=shorthand==3||(!shorthand&&(firmware_ap.icr_high>>24)==1);
  if(!delivery&&firmware_ap.handoff){
   if(!linux_fixed_ipi(0,firmware_ap.icr_high,value))return false;
  }else{
  if(!target||(value&~UINT32_C(0xcc7ff))||(delivery!=5&&delivery!=6))return false;
  if(delivery==5){
   if(value&255)return false;
   if((!(value&0x8000)||(value&0x4000))&&!firmware_ap_init())return false;
  }else if(firmware_ap.initialized){
   uint32_t vector=value&255;
   if(!FW_SWITCH(host.vmcs_page,firmware_ap.page))return false;
   bool ok=wr(0x0802,vector<<8)&&wr(0x6808,vector<<12)&&wr(0x681e,0);
   if(!FW_SWITCH(firmware_ap.page,host.vmcs_page))fatal();
   if(!ok)return false;
   firmware_ap.initialized=false;
   firmware_ap.runnable=true;
  }
  }
 }
 if((e->rflags&0x10000)&&!wr(0x6820,e->rflags&~UINT64_C(0x10000)))return false;
 if(!mov.write){
  uint32_t loaded=high?firmware_ap.icr_high:firmware_ap.icr_low;
  if(mov.reg==4){if(!wr(0x681c,loaded))return false;}
  else native.gpr[indices[mov.reg]]=loaded;
 }else if(high)firmware_ap.icr_high=value&0xff000000;
 else firmware_ap.icr_low=value;
 return wr(0x681e,mov.next_rip);
}

static void firmware_cpu_identity(uint32_t leaf,unsigned id,struct fcp_result *r){
 /* The LAPIC bus is 100 MHz. CPUID.15H describes a 10 MHz TSC
  * derived from that crystal, shared by both virtual CPUs. */
 if(leaf==0x15||(leaf>0x15&&leaf<0x80000000u)||leaf>0x80000008u){
  *r=(struct fcp_result){10,1,100000000,0};
 }
 if(leaf==1){
  r->b=(r->b&0xffff)|(2u<<16)|(id<<24);
  r->d|=1u<<28;
 }
}
static bool firmware_ap_mmio(void){
 static const unsigned indices[]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
 struct fat_exit e={.reason=48,.rip=firmware_ap.last_rip,.qualification=firmware_ap.last_qual};
 struct fmm_memory m={.memory=&mem,.ap_startup_cache_state=true,.advanced_ept_info=!!(host.caps.ept&(UINT64_C(1)<<22))};
 uint64_t ar,cs,vector,event;
 if(!rd(0x2400,&e.physical)||!rd(0x640a,&e.linear)||!rd(0x681c,&e.rsp)||
    !rd(0x6820,&e.rflags)||!rd(0x4824,&e.blocking)||!rd(0x6822,&e.pending_debug)||
    !rd(0x681a,&e.dr7)||!rd(0x4016,&event)||!rd(0x4408,&vector)||
    !rd(0x680c,&e.ds_base)||!rd(0x6806,&e.es_base)||!rd(0x680a,&e.ss_base)||
    !rd(0x6800,&m.cr0)||!rd(0x6802,&m.cr3)||!rd(0x6804,&m.cr4)||!rd(0x2806,&m.efer)||
    !rd(0x6808,&m.cs_base)||!rd(0x4816,&ar)||!rd(0x0802,&cs))return false;
 m.cs_ar=ar;m.cs_selector=cs;e.vectoring=vector&0x80000000;e.entry_event=event;
 firmware_ap.last_operand=e.physical;
 if((e.physical&~UINT64_C(4095))==0xfed00000)return linux_hpet(&m,&e,firmware_ap.native.gpr);
 struct guest_mmio_mov mov;
 if(!firmware_apic_mov(&m,&e,firmware_ap.native.gpr,&mov))return false;
 uint32_t offset=e.physical-0xfee00000,value=mov.store_value;
 firmware_ap.mmio_value=value;
 struct guest_apic_effect effect={0};
 if(!mov.write&&offset==0x20)value=1u<<24;
 /* The AP has no wired LINT pins in this topology. The legacy PIC is
  * connected exclusively to BSP LINT0. Preserve the programmed LVTs. */
 else if(firmware_ap.handoff){
  if(offset==0x310){
   if(mov.write)firmware_ap.apic.icr=((uint64_t)(value&0xff000000)<<32)|(uint32_t)firmware_ap.apic.icr;
   else value=firmware_ap.apic.icr>>32;
  }else if(offset==0x300){
   if(mov.write){
    if(!linux_fixed_ipi(1,firmware_ap.apic.icr>>32,value))return false;
    firmware_ap.apic.icr=(firmware_ap.apic.icr&~UINT64_C(0xffffffff))|value;
   }else value=(uint32_t)firmware_ap.apic.icr;
  }else if(!linux_lapic_register(&firmware_ap.apic,offset,mov.write,&value))return false;
 }
 else if(mov.write&&offset==0x350&&value==0x700)firmware_ap.apic.lvt[3]=value;
 else if(mov.write&&offset==0x360&&value==0x400)firmware_ap.apic.lvt[4]=value;
 else if(mov.write&&offset==0xf0&&!(value&~0x1ffu))firmware_ap.apic.irq.svr=value|15;
 else if(offset==0x320||offset==0x380||offset==0x390||offset==0x3e0){
  if(ft100_mmio_access(&firmware_ap.timer,active_apic.epoch,active_apic.reference,
                       !!(firmware_ap.apic.irq.svr&0x100),offset,4,mov.write,&value)!=FT100_OK)return false;
 }else if((mov.write&&offset==0x300)||
         !guest_apic_mmio(&firmware_ap.apic,offset,4,mov.write,&value,0,&effect))return false;
 if((e.rflags&0x10000)&&!wr(0x6820,e.rflags&~UINT64_C(0x10000)))return false;
 if(!mov.write){
  if(mov.reg==4){if(!wr(0x681c,value))return false;}
  else firmware_ap.native.gpr[indices[mov.reg]]=value;
 }
 return wr(0x681e,mov.next_rip);
}
static bool ap_reference(struct lhc_sample *s){
 return safe()&&clock_source.sample(clock_source.ctx,s)&&s->cpu==clock_source.properties.cpu&&
        s->epoch==active_apic.epoch&&s->tsc>=active_apic.reference&&
        s->tsc-active_apic.reference<=clock_source.max_gap;
}
static bool ap_cache_sample(void *unused){
 (void)unused;struct lhc_sample sample;return ap_reference(&sample);
}
static bool firmware_ap_exit(void){
 static const unsigned indices[]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
 uint64_t *g=firmware_ap.native.gpr;
 uint64_t q=firmware_ap.last_qual,reason=firmware_ap.last_reason,len;
 if(reason==48)return firmware_ap_mmio();
 if(reason==52)return wr(0x482e,UINT64_C(1)<<(28-host.caps.timer_shift));
 if(!rd(0x440c,&len)||!len||len>15)return false;
 struct fcm_state *current=&firmware_ap.mode,next;
 if(reason==14&&firmware_ap.handoff&&current->ia32e)return disk_cpu_invlpg(g,firmware_ap.last_rip,q,len);
#ifdef SYLE_M2_NESTED
 if(nested_enabled&&((reason>=19&&reason<=27)||reason==50||(reason==31&&nested_msr_supported(g[2])))){
  uint64_t flags,cs,block,vector;
  if(!rd(0x0802,&cs)||(cs&3)||!rd(0x6820,&flags)||!rd(0x4824,&block)||block||
     !rd(0x4408,&vector)||(vector&0x80000000))return false;
  if(reason==31)return nested_read_msr(g,firmware_ap.last_rip,len);
  return nested_root_instruction(reason,firmware_ap.last_rip,len,q,flags);
 }
#endif
 if(reason==28){
  unsigned cr=q&15,access=(q>>4)&3,index=(q>>8)&15;
  if((q&~UINT64_C(0xf3f))||access>1||(cr!=0&&cr!=3&&cr!=4))return false;
  enum fcm_register reg=cr==0?FCM_CR0:cr==3?FCM_CR3:FCM_CR4;
  if(access==0){
   uint64_t value;
   if(index==4){if(!rd(0x681c,&value))return false;}
   else value=g[indices[index]];
   firmware_ap.last_operand=value;
   if(!de_mode_write(current,reg,value,&next)||!commit_vcpu_mode(current,&next))return false;
  }else{
   uint64_t value=cr==0?current->cr0:cr==3?current->cr3:current->cr4;
   if(index==4){if(!wr(0x681c,value))return false;}
   else g[indices[index]]=value;
  }
 }else if(reason==54){
  uint64_t cs,secondary;
  uint8_t bytes[15];
  size_t available=io_instruction_fetch(firmware_ap.last_rip,bytes);
  if(!(current->cr0&1)){
   uint64_t base,limit;
   if(!rd(0x6808,&base)||!rd(0x4802,&limit)||firmware_ap.last_rip>=limit||
      base>FWM_RAM_SIZE-2||firmware_ap.last_rip>FWM_RAM_SIZE-base-2||
      !fwm_read_ram(&mem,base+firmware_ap.last_rip,bytes,2))return false;
   available=2;
  }
  bool attempted=false;
  if(!rd(0x0802,&cs)||!rd(0x401e,&secondary)||!(secondary&64)||!fwb_kvm_environment())return false;
  return fwb_apply(&firmware_ap.cache,reason,q,firmware_ap.last_rip,len,cs&3,
                   bytes,available,true,true,&attempted,ap_cache_sample,cache_maintain,pic_write);
 }else if(reason==29){
  struct fcmd_gate gate;
  uint64_t flags,cs,event,vector,blocking,regs[16];
  uint8_t bytes[15];
  size_t count=io_instruction_fetch(firmware_ap.last_rip,bytes);
  if(!de_gate(&gate)||!rd(0x0802,&cs)||(cs&3)||!current->ia32e||
     !rd(0x4016,&event)||event||!rd(0x4408,&vector)||(vector&0x80000000)||
     !rd(0x4824,&blocking)||blocking||!rd(0x6820,&flags)||!rd(0x681c,&regs[4]))return false;
  for(unsigned i=0;i<16;i++)if(i!=4)regs[i]=g[indices[i]];
  if(!fbd_apply(&firmware_ap.debug,regs,firmware_ap.last_rip,flags,q,len,bytes,count,0,irq_write))return false;
  for(unsigned i=0;i<16;i++)if(i!=4)g[indices[i]]=regs[i];
  return true;
 }else if(reason==12){
  uint64_t flags,block;
  if(len!=1||!rd(0x6820,&flags)||!rd(0x4824,&block)||block>2||
     (block&&!wr(0x4824,0)))return false;
  firmware_ap.halt_if=(flags&512)!=0;
  firmware_ap.runnable=false;
 }else if(reason==40){
  struct fpa_trace trace={0};
  return fpa_handle(&firmware_ap.pause,&mem,true,0,pause_read,pic_write,&trace)==FAT_COMMITTED;
 }else if(reason==30&&firmware_ap.handoff){
  unsigned port=q>>16;
  uint8_t bytes[15];size_t n=io_instruction_fetch(firmware_ap.last_rip,bytes);
  uint64_t ip=firmware_ap.last_rip,cs;
  if(!rd(0x0802,&cs)||(cs&3)||!fpp_encoding(q,len,bytes,n)||!disk_tick())return false;
  if(port>=0x3f8&&port<=0x3ff)return disk_uart_io(g,ip,q,len);
  if(port==0x22||port==0x23)return linux_imcr_io(g,ip,q,len);
  if(port==0x80)return lp80_io(&disk_post,g,ip,0,30,q,len,0,irq_write);
  if(port==0x20||port==0x21||port==0xa0||port==0xa1){
   uint64_t flags;
   return disk_pic_active&&rd(0x6820,&flags)&&lpd_ports(&disk_legacy,g,ip,q,len,flags,0,irq_write);
  }
  if(port==0xcfb)return persistent_cpu_config_probe(g,ip,q,len);
  if(disk_port(port))return disk_cpu_io(g,ip,q,len,false);
  if(port>=0xcfc&&port<=0xcff){
   uint32_t device=pci.pci.address&0x80ffff00;
   if(device==0x80001800)return disk_cpu_io(g,ip,q,len,true);
   if(device==0x80001000)return console_cpu_config(g,ip,q,len);
  }
  if(port==0xcf8||(port>=0xcfc&&port<=0xcff))
   return fpp_io(&pci,g,ip,0,q,len,active_apic.epoch,active_apic.reference,bytes,n,true,0,pic_write);
  return false;
 }else if(reason==16){
  uint64_t cs,flags;
  if(len!=2||!rd(0x0802,&cs)||((cs&3)&&(current->cr4&4))||!rd(0x6820,&flags))return false;
  struct lhc_sample sample;
  if(!ap_reference(&sample))return false;
  uint64_t value=sample.tsc-virtual_tsc.origin;
  g[0]=(uint32_t)value;g[3]=value>>32;
  if((flags&0x10000)&&!wr(0x6820,flags&~UINT64_C(0x10000)))return false;
 }else if(reason==10){
  struct fcp_result r;
  if(!fcats_query(&cpu,tsc_source_ready(),(uint32_t)g[0],(uint32_t)g[2],&r))return false;
  firmware_cpu_identity((uint32_t)g[0],1,&r);
#ifdef SYLE_M2_NESTED
  if(nested_enabled&&(uint32_t)g[0]==1)r.c|=1u<<5;
#endif
  g[0]=r.a;g[1]=r.b;g[2]=r.c;g[3]=r.d;
 }else if(reason==31||reason==32){
  uint32_t msr=g[2];
  firmware_ap.last_operand=msr;
  if(current->ia32e&&firmware_ap.handoff&&disk_linux_msr_supported(msr))return disk_cpu_msr(g,&firmware_ap.state,&firmware_ap.microcode,firmware_ap.last_rip,reason,len);
  if(msr==0x8b)return lmc_msr(&firmware_ap.microcode,g,firmware_ap.last_rip,0,reason,len,0,pic_write);
  if(msr==0x17&&reason==31){
   struct fcp_result signature;
   uint64_t value;
   if(!fcats_query(&cpu,tsc_source_ready(),1,0,&signature)||!fpid_read(signature.a,msr,&value))return false;
   g[0]=(uint32_t)value;g[3]=value>>32;
  }else if(msr==0xc0000080&&reason==32){
   uint64_t value=((uint64_t)(uint32_t)g[3]<<32)|(uint32_t)g[0];
   if(!de_mode_write(current,FCM_EFER,value,&next)||!commit_vcpu_mode(current,&next))return false;
  }else if(reason==31&&(msr==0xc0000080||msr==0x1b)){
   uint64_t value=msr==0x1b?UINT64_C(0xfee00800):current->efer;
   g[0]=(uint32_t)value;g[3]=value>>32;
  }else if(firmware_ap.handoff&&current->ia32e){
   uint64_t event,flags;
   if(!rd(0x4016,&event)||event||!rd(0x6820,&flags)||!wr(0x4018,0)||!wr(0x401a,0)||
      !wr(0x6820,flags|0x10000)||!wr(0x4016,0x80000b0d))return false;
   firmware_ap.gp_pending=true;
   return true;
  }else return false;
 }else return false;
 return wr(0x681e,firmware_ap.last_rip+len);
}

static bool linux_clock_read_exit(uint64_t reason){
 uint64_t flags,gpa,q;
 return rd(0x6820,&flags)&&!(flags&0x200)&&
        (reason==16||(reason==48&&rd(0x2400,&gpa)&&gpa==0xfed000f0&&
                      rd(0x6400,&q)&&(q&7)==1));
}
#ifdef SYLE_M2_NESTED
static bool linux_nested_register_exit(uint64_t reason){
 uint64_t info,flags;
 return (reason==23||reason==25)&&rd(0x440e,&info)&&(info&(1u<<10))&&
        rd(0x6820,&flags)&&!(flags&0x200);
}
#endif
static bool firmware_ap_step(enum lvs_result *bsp_result){
 if((!bsp_result&&!clock_sync())||ft100_sample(&firmware_ap.timer.timer,active_apic.epoch,active_apic.reference,0)!=FT100_OK)return false;
 if(!FW_SWITCH(host.vmcs_page,firmware_ap.page))return false;
#ifdef SYLE_M2_NESTED
 if(nested_enabled&&nested_contexts[1].entry.suspended){
  nested_current=&nested_contexts[1];
  bool resumed=nested_resume_slice();
  nested_current=nested_contexts;
  if(!FW_SWITCH(firmware_ap.page,host.vmcs_page))fatal();
  return resumed;
 }
#endif
 unsigned vector=0;
 if(firmware_ap.handoff&&!linux_lapic_before(&firmware_ap.apic,&vector)){
  if(!FW_SWITCH(firmware_ap.page,host.vmcs_page))fatal();
  return false;
 }
 uint64_t ticks=(UINT64_C(1)<<20)>>host.caps.timer_shift;
 if(firmware_ap.handoff&&!wr(0x482e,ticks?ticks:1)){
  if(!FW_SWITCH(firmware_ap.page,host.vmcs_page))fatal();
  return false;
 }
 /* Linux reads HPET, TSC and HPET with interrupts disabled. Preserve that
  * short read sequence on this CPU instead of charging an intervening BSP
  * run slot to the watchdog. Every exit is accounted before another entry. */
 bool ok=false;
 for(unsigned burst=0;burst<16;burst++){
  enum lvs_result run;
#ifdef SYLE_M2_HOST_PARALLEL
  if(bsp_result&&!burst)run=fmp_pair_run(bsp_result,false);
  else
#endif
  run=FW_AP_RUN();
  if(firmware_ap.state.host_dirty||!safe())fatal();
  ok=run==LVS_EXIT&&firmware_ap.native.entry_status==1&&
          rd(0x4402,&firmware_ap.last_reason)&&rd(0x6400,&firmware_ap.last_qual)&&
          rd(0x681e,&firmware_ap.last_rip)&&!(firmware_ap.last_reason>>16);
  if(ok&&firmware_ap.gp_pending){uint64_t v;ok=rd(0x4408,&v)&&!(v&0x80000000)&&wr(0x4016,0);firmware_ap.gp_pending=false;}
  if(ok&&vector){uint64_t v;ok=rd(0x4408,&v)&&linux_lapic_after(&firmware_ap.apic,vector,firmware_ap.last_reason,v);vector=0;}
  if(ok){
   firmware_ap.entries++;
   firmware_ap.native.resume=1;
#ifdef SYLE_M2_NESTED
   nested_current=&nested_contexts[1];
#endif
   ok=firmware_ap_exit();
#ifdef SYLE_M2_NESTED
   nested_current=nested_contexts;
#endif
  }
  if(!ok||!firmware_ap.handoff||!firmware_ap.runnable)break;
#ifdef SYLE_M2_NESTED
  /* A KVM timer callback gets back to L2 through a group of register
   * VMREAD/VMWRITE instructions. Do not run a BSP slot after each one. */
  if(nested_enabled&&nested_contexts[1].vmcs.active&&
     linux_nested_register_exit(firmware_ap.last_reason))continue;
#endif
  if(burst>=3||!linux_clock_read_exit(firmware_ap.last_reason))break;
 }
 if(!FW_SWITCH(firmware_ap.page,host.vmcs_page))fatal();
 return ok;
}
