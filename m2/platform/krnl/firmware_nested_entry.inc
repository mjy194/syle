/* Private VMCS02 per single-vCPU context and a composed EPT bound to the active EPTP12. VMCS12 host fields and
 * guest MSR-list addresses must never become physical VMCS host fields. */
static struct {
 uint64_t guest_cr0,guest_cr3,guest_cr4,eptp12,entry_count,exit_count,store_count;
 uint64_t reason,qualification,gpa,vm_instruction_error;
 uint64_t walk_status,fill_status,attempt,vectoring,exit_rip;
 uint64_t stage,field,en,sec,event,rip,link;
 bool entered;
} nested_entry;

#include "nested_ept.h"
#include "nested_ept_sparse.h"
struct nested_page_budget {uint64_t vmcs;unsigned pages;};
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
 (void)ctx;return ops.invept(ops.context,eptp);
}

/* L1 RAM remains pinned for the guest lifetime. Within one virtual EPTP
 * hardware may cache translations until INVEPT. No table survives a guest
 * stop, VMXOFF, VMCLEAR or backing release. */
static struct nested_ept_sparse composed;
static struct nested_page_budget composed_budget;
static uint64_t composed_source;
/* Both physical VMCS pages stay pinned to the same CPU. A virtual
 * VMCLEAR, VMXOFF or guest teardown ends the VMCS02 lifetime. */
static struct {
 uint64_t page,fields[LVP_MAX_FIELDS];
 bool launched;
} nested_cpu;
static void nested_drop_cpu(void){
 if(nested_cpu.page&&(!ops.vmclear(ops.context,nested_cpu.page)||
    !release(0,nested_cpu.page)))fatal();
 nested_cpu=(__typeof__(nested_cpu)){0};
 composed_budget.vmcs=0;
}
static void nested_drop_ept(void){
 if(composed.root&&!nested_ept_sparse_destroy(&composed))fatal();
 if(composed_budget.pages)fatal();
 composed_source=0;
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
 uint64_t cr0,cr3,cr4;
 if(!nvmx_peek(&nested_vmcs,0x6c00,&cr0)||!nvmx_peek(&nested_vmcs,0x6c02,&cr3)||
    !nvmx_peek(&nested_vmcs,0x6c04,&cr4)||cr0!=mode.cr0||cr3!=mode.cr3||cr4!=mode.cr4)
  return false;
 const uint32_t from[]={0x4c00,0x6c10,0x6c12,0x6c06,0x6c08,0x6c0a,0x6c0c,0x6c0e,0x6c14,0x6c16};
 const uint32_t to[]=  {0x482a,0x6824,0x6826,0x680e,0x6810,0x6814,0x6816,0x6818,0x681c,0x681e};
 for(unsigned i=0;i<sizeof from/sizeof *from;i++){
  uint64_t v;if(!nvmx_peek(&nested_vmcs,from[i],&v)||
   (i&&((v>>47)!=0&&(v>>47)!=0x1ffff))||!wr(to[i],v))return false;
 }
 for(unsigned i=0;i<8;i++){
  uint64_t sel=0;
  if(i!=6&&(!nvmx_peek(&nested_vmcs,0xc00+2*(i==7?6:i),&sel)||(sel&7)))return false;
  if((i==1||i==7)&&!sel)return false;
  uint64_t ar=i==1?0xa09b:i==7?0x8b:sel?0xc093:0x10000;
  if(!wr(0x800+2*i,sel)||!wr(0x4814+2*i,ar)||
     !wr(0x4800+2*i,i==7?103:UINT32_MAX))return false;
  if((i<4||i==6)&&!wr(0x6806+2*i,0))return false;
 }
 return wr(0x4810,0xffff)&&wr(0x4812,0xffff)&&wr(0x6820,2)&&wr(0x681a,0x400)&&
        wr(0x2802,0)&&wr(0x4824,0)&&wr(0x4826,0)&&wr(0x6822,0);
}

static bool nested_save_exit(void){
 if(!nvmx_entry_commit(&nested_vmcs))return false;
 for(unsigned i=0;i<host.plan.count;i++){
  uint32_t field=host.plan.fields[i].encoding;
  if(((field>>10)&3)!=2||field==0x482e||field==0x2804||field==0x2806)continue;
  uint64_t value;
  if(!rd(field,&value)||!nvmx_exit_store(&nested_vmcs,field,value))return false;
  nested_cpu.fields[i]=value;
 }
 const uint32_t exit_fields[]={0x4402,0x4404,0x4408,0x440c,0x440e};
 for(unsigned i=0;i<sizeof exit_fields/sizeof *exit_fields;i++){
  uint64_t value;
  if(!rd(exit_fields[i],&value)||!nvmx_exit_store(&nested_vmcs,exit_fields[i],value))return false;
 }
 uint64_t vectoring,vector_error;
 if(!rd(0x4408,&vectoring))return false;
 if((vectoring&0x80000800u)==0x80000800u&&
    (!rd(0x440a,&vector_error)||!nvmx_exit_store(&nested_vmcs,0x440a,vector_error)))return false;
 if(nested_entry.reason==0){
  uint64_t info,error;
  if(!rd(0x4404,&info))return false;
  if((info&(1u<<11))&&(!rd(0x4406,&error)||!nvmx_exit_store(&nested_vmcs,0x4406,error)))return false;
 }
 if(nested_entry.reason==49){
  if(!nvmx_exit_store(&nested_vmcs,0x4402,49)||!nvmx_exit_store(&nested_vmcs,0x2400,nested_entry.gpa))return false;
 }
 if(nested_entry.reason==48){
  uint64_t gla;
  if(!nvmx_exit_store(&nested_vmcs,0x2400,nested_entry.gpa))return false;
  if((nested_entry.qualification&128)&&(!rd(0x640a,&gla)||!nvmx_exit_store(&nested_vmcs,0x640a,gla)))return false;
 }
 return nvmx_exit_store(&nested_vmcs,0x6400,nested_entry.qualification)&&nvmx_return_from_l2(&nested_vmcs);
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

static bool nested_rdtsc(void){
 uint64_t len,rip,flags,cs,cr4,primary,offset=0;
 struct lhc_sample sample;
 if(!nvmx_peek(&nested_vmcs,0x4002,&primary)||
    !rd(0x440c,&len)||len!=2||!rd(0x681e,&rip)||!rd(0x6820,&flags)||
    !rd(0x0802,&cs)||!rd(0x6804,&cr4)||((cs&3)&&(cr4&4))||
    !tsc_source_ready()||!clock_source.sample(clock_source.ctx,&sample)||
    !ftsc_sample(&virtual_tsc,host.vmcs_page,sample.epoch,sample.cpu,sample.tsc))return false;
 if((primary&(1u<<3))&&!nvmx_peek(&nested_vmcs,0x2010,&offset))return false;
 uint64_t value=sample.tsc-virtual_tsc.origin+offset;
 if(!wr(0x681e,rip+len)||!wr(0x6820,flags&~UINT64_C(0x10000)))return false;
 native.gpr[0]=(uint32_t)value;native.gpr[3]=value>>32;
 return true;
}

/* The L2 i8259 masks IRQ0, issues specific EOI 0x60 and unmasks IRQ0
 * before returning. Measure only complete, uninterrupted sequences. */
static struct {
 uint64_t samples,begin,elapsed,over_period,vmx_ops,ops_begin,ops_total;
 bool pending,eoi;
} irq0_cost;
static void nested_irq0_report(void);
static bool nested_run_entry(void){
 nested_entry=(__typeof__(nested_entry)){0};
 if(!nvmx_peek(&nested_vmcs,0x6800,&nested_entry.guest_cr0)||
    !nvmx_peek(&nested_vmcs,0x6802,&nested_entry.guest_cr3)||
    !nvmx_peek(&nested_vmcs,0x6804,&nested_entry.guest_cr4)||
    !nvmx_peek(&nested_vmcs,0x201a,&nested_entry.eptp12)||
    !nvmx_peek(&nested_vmcs,0x4014,&nested_entry.entry_count)||
    !nvmx_peek(&nested_vmcs,0x4010,&nested_entry.exit_count)||
    !nvmx_peek(&nested_vmcs,0x400e,&nested_entry.store_count))return false;
 uint64_t en,secondary,event,flags,rip,link;
 if(!nvmx_peek(&nested_vmcs,0x4012,&en)||!nvmx_peek(&nested_vmcs,0x401e,&secondary)||
    !nvmx_peek(&nested_vmcs,0x4016,&event)||!nvmx_peek(&nested_vmcs,0x6820,&flags)||
    !nvmx_peek(&nested_vmcs,0x681e,&rip)||!nvmx_peek(&nested_vmcs,0x2800,&link))return false;
 nested_entry.en=en;nested_entry.sec=secondary;nested_entry.event=event;nested_entry.rip=rip;nested_entry.link=link;
 nested_entry.stage=1;
 if((event&0x800007ffu)==0x80000030u){
  irq0_cost.pending=true;irq0_cost.eoi=false;
  irq0_cost.begin=active_apic.reference;irq0_cost.ops_begin=irq0_cost.vmx_ops;
 }
 /* The virtual controls omit LOAD_EFER. For IA-32e entry, inherit
  * L1's guest EFER and set LME/LMA in the L0-owned VMCS02. */
 const uint32_t ia32e_entry=1u<<9;
 bool long_mode=(en&ia32e_entry)!=0;
 uint64_t cs_ar,dr7,debugctl;
 if(!nvmx_peek(&nested_vmcs,0x4816,&cs_ar)||!nvmx_peek(&nested_vmcs,0x681a,&dr7)||
    !nvmx_peek(&nested_vmcs,0x2802,&debugctl)||debugctl||(dr7&~UINT64_C(0x400)))return false;
 if(long_mode&&((cs_ar&0x16098)!=0x2098||(flags&(1u<<17))))return false;
 /* Long-mode fetches remain constrained by guest paging and EPT02. */
 if(long_mode){
  if(((rip>>47)!=0&&(rip>>47)!=0x1ffff)||
     (nested_entry.guest_cr0&~UINT64_C(0x5000e))!=0x80000031||
     (nested_entry.guest_cr4&~UINT64_C(0x6d8))!=0x2020)return false;
 }else if(rip>=4096||nested_entry.guest_cr0!=0x80000031||nested_entry.guest_cr4!=0x2011)return false;
 if((en&~(4u|ia32e_entry))||secondary!=2||!nested_event_valid(event)||link!=UINT64_MAX||
    (nested_entry.guest_cr3&~UINT64_C(0xfffff000))||
    (nested_entry.eptp12&4095)!=0x1e||(nested_entry.eptp12>>32)||
    nested_entry.entry_count||nested_entry.exit_count||nested_entry.store_count)return false;
 static const uint32_t host_fields[]={
  0xc00,0xc02,0xc04,0xc06,0xc08,0xc0a,0xc0c,0x4c00,0x2c00,0x2c02,
  0x6c00,0x6c02,0x6c04,0x6c06,0x6c08,0x6c0a,0x6c0c,0x6c0e,0x6c10,0x6c12,0x6c14,0x6c16};
 uint64_t host_values[sizeof host_fields/sizeof *host_fields];
 bool fresh=!nested_cpu.page,loaded=false,reflect=false;
 nested_entry.stage=2;
 if(composed_source!=nested_entry.eptp12)nested_drop_ept();
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
  if(!ops.vmclear(ops.context,page))fatal();
  nested_cpu.page=composed_budget.vmcs=page;
 }
 const struct nested_sparse_ops sparse_ops={nested_page_alloc,nested_page_free,nested_flush};
 if(!composed.root&&!nested_ept_sparse_init(&composed,&sparse_ops,&composed_budget,ops.physical_bits))goto done;
 composed_source=nested_entry.eptp12;
 if(!lvb_switch_current(&backend,host.vmcs_page,nested_cpu.page))goto done;
 loaded=true;nested_entry.stage=3;
 if(fresh)for(unsigned i=0;i<sizeof host_fields/sizeof *host_fields;i++)
  if(!wr(host_fields[i],host_values[i]))goto done;
 for(unsigned i=0;i<host.plan.count;i++){
  uint32_t f=host.plan.fields[i].encoding;
  nested_entry.field=f;
  uint64_t v=host.plan.fields[i].value;
  /* Guest-state fields only. Physical controls always come from the L0 plan. */
  if(((f>>10)&3)==2 && f!=0x482e && f!=0x2804 && f!=0x2806){
   if(!nvmx_peek(&nested_vmcs,f,&v))goto done;
   if(f==0x6804)v|=0x2000;
  }
  /* Page-table roots resolve through EPT02. Honor KVM's CR3, INVLPG,
   * interrupt-window and PAUSE controls; retain L0's other intercepts. */
  if(long_mode&&f==0x4002){
   uint64_t primary;
   if(!nvmx_peek(&nested_vmcs,f,&primary))goto done;
   const uint32_t selective_exits=(1u<<2)|(1u<<9)|(1u<<15)|(1u<<16)|(1u<<30);
   v=(v&~selective_exits)|(primary&selective_exits);
  }
  if(long_mode&&(f==0x6000||f==0x6002||f==0x6004||f==0x6006))
   if(!nvmx_peek(&nested_vmcs,f,&v))goto done;
  if(long_mode&&f==0x4012)v|=ia32e_entry;
  if(long_mode&&f==0x2806)v=mode.efer|0x500;
  if(f==0x4016)v=(event&(1u<<31))?event:0;
  if(f==0x401a&&(event&0x80000700u)==0x80000600u&&
     (!nvmx_peek(&nested_vmcs,f,&v)||!v||v>15))goto done;
  if(f==0x4018&&(event&(1u<<11))&&!nvmx_peek(&nested_vmcs,f,&v))goto done;
  if(f==0x201a)v=composed.eptp;
  if(f==0x4004)v=UINT32_MAX;
  if(long_mode&&(f==0x4004||f==0x4006||f==0x4008))
   if(!nvmx_peek(&nested_vmcs,f,&v))goto done;
  /* Event delivery, the timer and hardware-saved MSRs may have changed
   * outside the reflected VMCS12 state. Always refresh those fields. */
  if(fresh||nested_cpu.fields[i]!=v||f==0x482e||f==0x2804||f==0x2806||
     f==0x4016||f==0x4018||f==0x401a){
   if(!wr(f,v))goto done;
   nested_cpu.fields[i]=v;
  }
 }
 nested_entry.stage=4;
 uint32_t previous_resume=native.resume;
 for(unsigned attempt=0;attempt<(long_mode?32768u:32u);attempt++){
  nested_entry.attempt=attempt;native.resume=nested_cpu.launched;
  enum lvs_result outcome=lvs_run(&state,&state_ops);
  native.resume=previous_resume;
  if(!safe())fatal();
  if(outcome!=LVS_EXIT||native.entry_status!=1){
   rd(0x4400,&nested_entry.vm_instruction_error);break;
  }
  nested_cpu.launched=true;
  if(!rd(0x4402,&nested_entry.reason)||!rd(0x6400,&nested_entry.qualification))break;
  nested_entry.entered=!(nested_entry.reason>>31);
  if(!nested_entry.entered)break;
  if(nested_entry.reason==29){
   uint64_t primary,regs[16],ip,fl,sz,cs;
   if(!nvmx_peek(&nested_vmcs,0x4002,&primary))break;
   if(primary&(1u<<23)){reflect=nested_save_exit();break;}
   if(!rd(0x0802,&cs)||(cs&3)||!rd(0x681e,&ip)||!rd(0x6820,&fl)||!rd(0x440c,&sz))break;
   for(unsigned i=0;i<16;i++)if(!reg_read(i,&regs[i]))goto done;
   if(!fbd_apply_qualified(&disk_debug_bank,regs,ip,fl,nested_entry.qualification,sz,0,irq_write))break;
   unsigned reg=(nested_entry.qualification>>8)&15;
   if((nested_entry.qualification&16)&&reg!=4&&!reg_write(reg,regs[reg]))break;
   if(!wr(0x4016,0))break;
   continue;
  }
  if(nested_entry.reason==16){
   uint64_t primary;
   if(!nvmx_peek(&nested_vmcs,0x4002,&primary))break;
   if(primary&(1u<<12)){reflect=nested_save_exit();break;}
   if(!nested_rdtsc()||!wr(0x4016,0))break;
   continue;
  }
  if(nested_entry.reason!=48){
   if(nested_entry.reason==7||nested_entry.reason==30||nested_entry.reason==12||nested_entry.reason==10||
      nested_entry.reason==14||nested_entry.reason==28||nested_entry.reason==31||nested_entry.reason==32)reflect=nested_save_exit();
   if(nested_entry.reason==0){
    uint64_t info,bitmap;
    if(rd(0x4404,&info)&&(info&0x800007ffu)==0x8000030du&&
       nvmx_peek(&nested_vmcs,0x4004,&bitmap)&&(bitmap&(1u<<13)))reflect=nested_save_exit();
   }
   break;
  }
  if(!rd(0x2400,&nested_entry.gpa))break;
  struct nested_ept_policy policy={32,false,false};
  uint64_t base=nested_entry.gpa&~UINT64_C(4095);
  struct nested_ept_result resolved=nested_ept_walk_memory_classified(nested_entry.eptp12,base,
    1,nested_ram_page,0,&policy);
  resolved=nested_sparse_walk_access(resolved,nested_entry.qualification&7);
  nested_entry.walk_status=resolved.status;
  if(resolved.status==NESTED_EPT_MISCONFIG){
   /* KVM encodes MMIO in deliberately misconfigured EPT12 entries. */
   nested_entry.reason=49;nested_entry.qualification=0;
   reflect=nested_save_exit();break;
  }
  if(resolved.status==NESTED_EPT_L1_DENIED){
   nested_entry.qualification=(nested_entry.qualification&0x187)|((uint64_t)resolved.effective12<<3);
   reflect=nested_save_exit();break;
  }
  if(resolved.status!=NESTED_EPT_OK)break;
  /* Keep the composed EPT bounded as Linux touches fresh pages. */
  if(composed_budget.pages>502&&!nested_ept_sparse_reset(&composed))break;
  nested_entry.fill_status=nested_ept_sparse_fill(&composed,base,&resolved);
  if(nested_entry.fill_status==NESTED_SPARSE_EXISTS){
   /* L1 can relax a cached read-only SPTE without an explicit flush. */
   if(!nested_ept_sparse_reset(&composed))break;
   nested_entry.fill_status=nested_ept_sparse_fill(&composed,base,&resolved);
  }
  if(nested_entry.fill_status!=NESTED_SPARSE_OK)break;
  uint64_t vectoring,err;
  if(!rd(0x4408,&vectoring))break;
  nested_entry.vectoring=vectoring;
  if(!(vectoring&(1u<<31)))vectoring=0;
  if(!nested_event_valid(vectoring))break;
  if((vectoring&(1u<<11))&&(!rd(0x440a,&err)||!wr(0x4018,err)))break;
  if((vectoring&0x80000700u)==0x80000600u){
   uint64_t len;
   if(!rd(0x440c,&len)||!len||len>15||!wr(0x401a,len))break;
  }
  if(!wr(0x4016,vectoring))break;
 }
done:
 if(reflect&&irq0_cost.pending&&nested_entry.reason==30){
  uint64_t q=nested_entry.qualification;
  if((q&0xffff003f)==0x00200000&&(uint8_t)native.gpr[0]==0x60)irq0_cost.eoi=true;
  if(irq0_cost.eoi&&(q&0xffff003f)==0x00210000&&!(native.gpr[0]&1)){
   uint64_t ticks=active_apic.reference-irq0_cost.begin;
   irq0_cost.pending=false;irq0_cost.elapsed+=ticks;irq0_cost.samples++;
   irq0_cost.ops_total+=irq0_cost.vmx_ops-irq0_cost.ops_begin;
   if(ticks>=40000)irq0_cost.over_period++;
   if(irq0_cost.samples==256||irq0_cost.samples==1024)nested_irq0_report();
  }
 }
 if(loaded){
  if(!reflect)rd(0x681e,&nested_entry.exit_rip);
  if(!lvb_switch_current(&backend,nested_cpu.page,host.vmcs_page))fatal();
 }
 if(reflect)reflect=nested_reflect_host();
 if(!reflect){nested_drop_cpu();nested_drop_ept();}
 return reflect;
}
