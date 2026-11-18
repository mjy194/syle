#include "linux_port80.h"
#include "firmware_boot_debug.h"
#include "linux_sysenter_msr.h"
#include "linux_pit2.h"
#include "firmware_boot_invlpg.h"
#include "linux_microcode_model.h"
#include "linux_msr_bank.h"
#include "firmware_boot_cpu.h"
#include "linux_gs_commit.h"
#include "firmware_irq_lvt_mask.h"
#include "firmware_disk.h"
#include "virtio_blk/firmware_ram_adapter.h"
static const uint8_t*disk_bytes;static size_t disk_size;
static struct vrt disk_state;static struct vfr disk_ram;
static uint64_t disk_notifies,disk_completions;
bool firmware_disk_bind(const void*p,size_t n){
 if(disk_bytes||!p||n!=64u*1024u*1024u)return false;
 disk_bytes=p;disk_size=n;return true;
}
static bool disk_port(uint16_t p){return disk_state.bar&&!disk_state.probe&&(disk_state.command&1)&&p>=disk_state.bar&&(uint32_t)p-disk_state.bar<64;}
static bool disk_dispatch(uint64_t rip,uint64_t q,unsigned len,bool config){
 uint8_t b[15];unsigned available=(unsigned)io_instruction_fetch(rip,b);
 unsigned size=q&7,width=size==3?4:size==1?2:1,port=(q>>16)&65535;
 bool input=(q&8)!=0;
 if(disk_state.failed||!irq_identity()||!observation_ok()||!fpp_encoding(q,len,b,available)||
    (!(q&64)&&(uint16_t)native.gpr[3]!=port))return false;
 struct vrt next=disk_state;uint32_t value=(uint32_t)native.gpr[0];bool ok;
 if(width<4)value&=(1u<<(width*8))-1;
 if(config){
  unsigned off=(pci.pci.address&252)+(port-0xcfc);
  if(!input&&off==16&&width==4&&value!=UINT32_MAX){
   uint32_t base=value&0xffffffc0u;
   if(base&&(base<0xc000||base>0xdfc0||(base<console_state.bar+8&&console_state.bar<base+64)))return false;
  }
  ok=vrt_config(&next,off,width,!input,&value);
 }else{
  unsigned off=port-next.bar;
  if(!input&&off==8&&value){
   uint8_t a[2],u[2];uint64_t base=(uint64_t)value<<12;
   if(!next.io.read(next.io.ctx,base+2050,a,2)||!next.io.read(next.io.ctx,base+4098,u,2)||a[0]||a[1]||u[0]||u[1])return false;
  }
  ok=vrt_io(&next,(uint16_t)port,width,!input,&value);
 }
 if(!ok||!pic_write(0,0x681e,rip+len)){disk_state.failed=true;disk_state.backend.failed=true;vfr_abort(&disk_ram);return false;}
 if(!config&&!input&&port-next.bar==16){disk_notifies++;disk_completions=disk_ram.publications;}
 disk_state=next;
#ifdef SYLE_FIRMWARE_PERSISTENT
 if(!persistent_irq(vrwt_irq(&disk_state))){disk_state.failed=true;return false;}
#endif
 if(input){uint64_t mask=width==4?UINT32_MAX:(UINT64_C(1)<<(width*8))-1;native.gpr[0]=width==4?value:(native.gpr[0]&~mask)|value;}
 return true;
}

static bool disk_clock_ready(void){return clock_source.sample&&clock_source.properties.verified&&clock_source.properties.invariant&&clock_source.properties.hz==10000000&&active_apic.epoch==clock_source.properties.epoch;}
static struct lmc_state disk_microcode;
static bool disk_read_field(void*x,uint32_t field,uint64_t*out){(void)x;return rd(field,out);}
static bool disk_linux_msr_supported(uint32_t id){
 return id==0x3a||id==0x140||(id>=0x174&&id<=0x176)||id==0x8b||id==0xc0000100u||id==0xc0000101u||id==0xc0000102u||(id>=0xc0000081u&&id<=0xc0000084u);
}
static bool disk_gs_msr(uint64_t rip,unsigned reason,unsigned len){
 uint8_t bytes[15];size_t n=io_instruction_fetch(rip,bytes);uint64_t gs;
 if(len!=2||n<2||bytes[0]!=0x0f||bytes[1]!=(reason==31?0x32:0x30)||!rd(0x6810,&gs))return false;
 uint32_t id=(uint32_t)native.gpr[2];
 if(id==0xc0000101)return lgsc_apply(native.gpr,rip,gs,0,reason,len,0,pic_write);
 if(id==0x8b)return lmc_msr(&disk_microcode,native.gpr,rip,0,reason,len,0,pic_write);
 struct fcp_result basic,ext;uint64_t fs;
 if(!fboot_query(&cpu,disk_clock_ready(),1,0,&basic)||!fboot_query(&cpu,disk_clock_ready(),0x80000001,0,&ext)||!rd(0x680e,&fs))return false;
 if(id>=0x174&&id<=0x176)return lse_apply(native.gpr,rip,0,reason,len,basic.d,ext.d,0,disk_read_field,0,pic_write);
 if(id==0x3a||id==0x140){
  /* Feature-control is locked for the lifetime of this guest. */
  if(basic.c||basic.a!=0x600||(id==0x3a&&reason!=31)||(reason==32&&((uint32_t)native.gpr[0]||(uint32_t)native.gpr[3])))return false;
  if(!pic_write(0,0x681e,rip+len))return false;
  if(reason==31){
   native.gpr[0]=id==0x3a?1:0;
#ifdef SYLE_M2_NESTED
   if(id==0x3a&&nested_enabled)native.gpr[0]|=4; /* Locked, VMX permitted outside SMX. */
#endif
   native.gpr[3]=0;
  }
  return true;
 }
 return lmbr_apply(native.gpr,rip,0,reason,len,basic.d,ext.d,fs,&state,0,pic_write);
}
static bool disk_cr_encoding(uint64_t rip,uint64_t q,unsigned len){
 uint8_t bytes[15];size_t n=io_instruction_fetch(rip,bytes);return fboot_cr_encoding(q,len,bytes,n);
}
static bool disk_invlpg(uint64_t rip,uint64_t linear,unsigned len){
 if(rip>UINT64_MAX-len||(((rip+len)>>47)!=0&&((rip+len)>>47)!=0x1ffff))return false;
 uint8_t bytes[15];size_t n=io_instruction_fetch(rip,bytes);uint64_t regs[16],primary,secondary,rsp,eptp;
 static const unsigned rm[16]={0,2,3,1,15,6,4,5,7,8,9,10,11,12,13,14};
 if(!irq_identity()||!rd(0x681c,&rsp)||!rd(0x4002,&primary)||!(primary&0x200)||
    !rd(0x401e,&secondary)||secondary!=0xc2||!rd(0x201a,&eptp)||eptp!=mem.eptp)return false;
 for(unsigned i=0;i<16;i++)regs[i]=i==4?rsp:native.gpr[rm[i]];
 if(!fbinv_decode(bytes,n,len,regs,linear)||!ops.invept(ops.context,mem.eptp))return false;
 /* VPID disabled: next entry invalidates guest linear/combined caches. Never
  * issue native INVLPG against the operand in the host address space. */
 return pic_write(0,0x681e,rip+len);
}

static struct lpit2 disk_pit2;static bool disk_pit2_ready;
static bool disk_pit2_io(uint64_t rip,uint64_t q,unsigned len){
 uint8_t bytes[15];unsigned n=(unsigned)io_instruction_fetch(rip,bytes);
 if(!fpp_encoding(q,len,bytes,n)||!clock_sync()||!irq_identity()||active_apic.reference<active_apic.clock.timer.origin)return false;
 uint64_t delta=active_apic.reference-active_apic.clock.timer.origin;
 if(delta/10000000>UINT64_MAX/LPIT_HZ)return false;
 uint64_t tick=(delta/10000000)*LPIT_HZ+(delta%10000000)*LPIT_HZ/10000000;
 if(!disk_pit2_ready){if(!lpit2_init(&disk_pit2,tick))return false;disk_pit2_ready=true;}
 return lpit2_io(&disk_pit2,native.gpr,rip,0,30,q,len,tick,0,pic_write)==LPIT_COMMITTED;
}

static struct fbd_bank disk_debug_bank;
static bool disk_debug(uint64_t rip,uint64_t q,unsigned len,uint64_t flags,uint64_t block,uint64_t pending,uint32_t vector){
 (void)debug_read_dispatch;
 struct fcmd_gate gate;uint64_t event,r[16];uint8_t b[15];size_t n=io_instruction_fetch(rip,b);
 if(!clock_sync()||!irq_identity()||!observation_ok()||!de_gate(&gate)||!debug_bank.ready||debug_bank.failed||
 block||pending||vector||!rd(0x4016,&event)||event)return false;
 static const unsigned map[16]={0,2,3,1,0,6,4,5,7,8,9,10,11,12,13,14};
 for(unsigned i=0;i<16;i++){if(i==4){if(!rd(0x681c,&r[i]))return false;}else r[i]=native.gpr[map[i]];}
 if(!fbd_apply(&disk_debug_bank,r,rip,flags,q,len,b,n,0,irq_write)){if(disk_debug_bank.failed)debug_bank.failed=true;return false;}
 for(unsigned i=0;i<16;i++)if(i!=4)native.gpr[map[i]]=r[i];
 return true;
}

#include "linux_pic04_domain.h"
#include "linux_pic04_runtime.h"
static struct lpd_domain disk_legacy;
static struct lpr_runtime disk_pic_run;
static bool disk_legacy_ready;
static uint64_t disk_pic_receipts,disk_irq0_receipts,disk_irq4_receipts,disk_rx_sent,disk_rx_read;
#ifdef SYLE_FIRMWARE_PERSISTENT
static uint64_t disk_irq5_receipts;
#endif
static bool disk_tick(void){
 if(!disk_legacy_ready)return true;
 if(active_apic.reference<active_apic.clock.timer.origin)return false;
 uint64_t d=active_apic.reference-active_apic.clock.timer.origin;
 if(d/10000000>UINT64_MAX/LPIT_HZ)return false;
 uint64_t t=d/10000000*LPIT_HZ+d%10000000*LPIT_HZ/10000000;
 return lpd_tick(&disk_legacy,t);
}
static bool disk_legacy_init(void){
 if(disk_legacy_ready||!lpd_init(&disk_legacy,(uint64_t)(uintptr_t)&disk_legacy))return false;
 disk_legacy_ready=true;return disk_tick();
}
static bool disk_pic_route(void){
 if(disk_pic_active)return true;
 if(!disk_legacy_ready||!irq_identity()||!flq_pic_quiet(&pic)||!irq_session.ready||
 irq_session.failed||irq_session.busy||irq_session.active_attempt||irq_session.event.phase!=LEI_IDLE||
 !irq_session.owner.masked||irq_session.owner.pending||irq_session.owner.in_service||irq_session.owner.reserved||
 irq_window.armed||halt_state.halted||!lpd_valid(&disk_legacy)||disk_legacy.master.initialized||
 disk_legacy.master.irr||disk_legacy.master.isr||disk_legacy.master.reserved.serial)return false;
 /* ICW1 reinitializes the cold source-free master. Preserve the slave's live
  * masks/configuration; PIT and UART state remain in this same device domain. */
 disk_legacy.slave=pic;disk_legacy.slave.master=255;
 if(!lpr_init(&disk_pic_run,&disk_legacy.master,host.caps.primary)||!fag_lint0_route_enable(&active_apic.device)||
 !irq_write(0,0x4004,0x42))return false;
 /* Normal guest exceptions, including Linux's INT3 alternatives self-test,
  * are delivered by VMX. #DB remains intercepted: no active debug support.
  * #UD stays a terminal unsupported-instruction diagnostic in this profile. */
 disk_pic_active=true;
 log_text("OVMF.DISK PIC_ExtINT_route enabled old_APIC_timer_masked_no_pending1\n");return true;
}
static bool disk_pic_entry_ok(uint64_t flags){
 return disk_pic_active&&disk_legacy_ready&&!disk_pic_run.failed&&
 active_apic.device.lint0_extint_routed&&!active_apic.device.lint0_extint_deasserted&&
 active_apic.device.apic.lvt[3]==0x700&&(active_apic.device.apic.irq.svr&0x100)&&
 irq_session.owner.masked&&!irq_session.owner.pending&&!irq_session.owner.in_service&&!irq_session.owner.reserved&&
 !irq_window.armed&&lpd_entry_ok(&disk_legacy,flags);
}
static struct lp80_state disk_post;
static bool disk_legacy_io(uint64_t rip,uint64_t q,unsigned len,uint64_t flags){
 uint8_t b[15];size_t n=io_instruction_fetch(rip,b);unsigned p=q>>16;
 if(!fpp_encoding(q,len,b,n)||!irq_identity()||!clock_sync()||!disk_tick())return false;
 if(p==0x80)return lp80_io(&disk_post,native.gpr,rip,0,30,q,len,0,irq_write);
 /* Explicit unpopulated ISA serial slots: open-bus reads and ignored writes. */
 if((p>=0x2f8&&p<=0x2ff)||(p>=0x3e8&&p<=0x3ef)||(p>=0x2e8&&p<=0x2ef)){
  if((q&7)||!irq_write(0,0x681e,rip+len))return false;
  if(q&8)native.gpr[0]=(native.gpr[0]&~UINT64_C(255))|255;
  return true;
 }
 if(p>=0x3f8&&p<=0x3ff){
  bool read_rx=(q&8)&&p==0x3f8&&!(disk_legacy.uart.uart.lcr&128)&&disk_legacy.uart.uart.rc;
  if(!lpd_uart_io(&disk_legacy,native.gpr,rip,q,len,0,irq_write))return false;
  if(read_rx)disk_rx_read++;
  bool available;uint8_t byte;
  if(!lpd_recv(&disk_legacy,&available,&byte))return false;
  if(available){if(console_count==sizeof console_output)console_count=0;console_output[console_count++]=byte;console_live_byte(byte);}
  return true;
 }
 if(p==0x40||p==0x43)return lpd_pit_io(&disk_legacy,native.gpr,rip,q,len,disk_legacy.timer.tick,0,irq_write);
 if(!disk_pic_active&&(!(q&8)&&p==0x20&&(uint8_t)native.gpr[0]==0x11)&&!disk_pic_route())return false;
 if(!disk_pic_active)return false;
 return lpd_ports(&disk_legacy,native.gpr,rip,q,len,flags,0,irq_write);
}
static bool disk_pic_before(bool*staged){
 struct lei_snapshot snap;uint32_t primary;uint64_t value;
 if(!disk_pic_entry_ok(0)||!irq_snapshot(&snap,&primary)||
 !lpr_before(&disk_pic_run,&disk_legacy.master,&snap,0,disk_read_field,irq_write)||!rd(0x4002,&value))return false;
 disk_pic_window=(value&4)!=0;
 *staged=disk_pic_run.irq.event.phase==LEI_STAGED;
 if(!rd(0x4016,&value)||value!=(*staged?(UINT64_C(0x80000000)|disk_pic_run.irq.event.vector):0))return false;
 return true;
}
static bool disk_pic_after(bool proven,uint32_t reason,uint32_t vector,bool*receipt){
 bool staged=disk_pic_run.irq.event.phase==LEI_STAGED;unsigned source=disk_pic_run.irq.ticket.source;
 if(!lpr_after(&disk_pic_run,&disk_legacy.master,proven?PMG_EXIT:PMG_UNKNOWN,reason,vector,0,irq_write))return false;
 *receipt=staged;if(staged){disk_pic_receipts++;if(source==0)disk_irq0_receipts++;else if(source==4)disk_irq4_receipts++;
#ifdef SYLE_FIRMWARE_PERSISTENT
 else if(source==5)disk_irq5_receipts++;
#endif
 else return false;}
 return true;
}
static bool disk_input(void){
 if(!disk_pic_active)return console_poll();
 if(disk_legacy.uart.uart.rc)return true;
 uint8_t status;__asm__ volatile("inb %1,%0":"=a"(status):"Nd"((uint16_t)0x3fd));
 if(status==255||(status&0x1e))return false;
 if(status&1){uint8_t b;bool accepted;__asm__ volatile("inb %1,%0":"=a"(b):"Nd"((uint16_t)0x3f8));if(!lpd_send(&disk_legacy,b,&accepted)||!accepted)return false;disk_rx_sent++;}
 return true;
}

#include "firmware_boot_gp.h"
static struct fbg_state disk_gp;
static bool disk_gp_pending(void){return disk_gp.pending;}
static bool disk_inject_gp(uint64_t rip,unsigned len){
 struct fbg_exit e={.rip=rip};struct fmm_memory m={.memory=&mem};uint64_t ar,cs,v;
 const uint32_t zero[]={0x4016,0x4018,0x401a,0x4824,0x4826,0x6822};
 if(len!=2||(uint32_t)native.gpr[2]!=0xce||!irq_identity()||!disk_pic_active||
 disk_pic_run.inflight||disk_pic_run.irq.event.phase!=LEI_IDLE||disk_legacy.master.reserved.serial||
 irq_window.armed||disk_pic_window||halt_state.halted)return false;
 for(unsigned i=0;i<sizeof zero/sizeof*zero;i++)if(!rd(zero[i],&v)||v)return false;
 if(!rd(0x6800,&m.cr0)||!rd(0x6802,&m.cr3)||!rd(0x6804,&m.cr4)||!rd(0x2806,&m.efer)||
 !rd(0x6808,&m.cs_base)||!rd(0x4816,&ar)||ar>UINT32_MAX||!rd(0x0802,&cs)||cs>UINT16_MAX||
 !rd(0x681c,&e.rsp)||!rd(0x6818,&e.idtr)||!rd(0x4812,&e.idt_limit)||!rd(0x6816,&e.gdtr)||!rd(0x4810,&e.gdt_limit)||!rd(0x6820,&e.flags))return false;
 m.cs_ar=(uint32_t)ar;m.cs_selector=(uint16_t)cs;
 bool ok=fbg_inject(&disk_gp,&m,&e,0,irq_write);
 log_text("OVMF.DISK unsupported_MSR_CE_GP injected=");log_hex(ok);log_text(" count=");log_hex(disk_gp.count);log_text("\n");return ok;
}
static bool disk_gp_entry(void){
 uint64_t v;
 if(disk_gp.failed)return false;
 if(!disk_gp.pending)return true;
 if(!irq_identity()||disk_pic_run.inflight||disk_pic_run.irq.event.phase!=LEI_IDLE||disk_legacy.master.reserved.serial||disk_pic_window||irq_window.armed||
 !rd(0x4016,&v)||v!=0x80000b0d||!rd(0x4018,&v)||v||!rd(0x401a,&v)||v||!rd(0x4826,&v)||v||!rd(0x4824,&v)||v)return false;
 return true;
}
static bool disk_gp_after(bool proven,uint32_t reason,uint32_t vector,uint32_t interruption){
 return proven&&irq_identity()&&fbg_after(&disk_gp,reason,vector,interruption,0,irq_write);
}

static bool disk_cs_identity(uint64_t cs,uint64_t ar){
 if(!(cs&7))return (ar&~UINT64_C(1))==0xa09a;
 uint64_t base,limit,ss,ssar;
 return disk_pic_active&&(cs&7)==3&&(ar&~UINT64_C(1))==0xa0fa&&
 rd(0x6808,&base)&&!base&&rd(0x4802,&limit)&&limit==UINT32_MAX&&
 rd(0x0804,&ss)&&ss&&ss<=UINT16_MAX&&(ss&7)==3&&rd(0x4818,&ssar)&&ssar==0xc0f3&&
 rd(0x680a,&base)&&!base&&rd(0x4804,&limit)&&limit==UINT32_MAX;
}
