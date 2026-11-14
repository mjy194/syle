#define main old_regression
#include "linux_pic04_fixture.c"
#undef main
#include "linux_pic04_idle.h"
#include "linux_progress_diag.h"
static void setup04(void){
 setup(false);struct lgd_config c=dispatch.config;c.legacy_pic04_enabled=true;c.legacy_idle_advance=true;c.sync_irq=0;c.irq_ctx=0;c.max_exits=10000;
 dispatch=(struct lgd_context){0};struct lgm_service_ops ops;assert(lgd_init(&dispatch,&c,&ops));
 session.host.caps.primary=UINT64_C(0xffffffff00000000);mock.f[0x4002]=0x80000080;
 assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=52};assert(lgd_after_entry(&dispatch,&session,&r));
}
static bool outp(unsigned port,uint8_t v){session.native.gpr[0]=v;session.native.gpr[3]=port;mock.f[0x6400]=((uint64_t)port<<16)|(port<256?64:0);mock.f[0x440c]=port<256?2:1;return run(30);}
static uint8_t inp(unsigned port){session.native.gpr[3]=port;mock.f[0x6400]=((uint64_t)port<<16)|8|(port<256?64:0);mock.f[0x440c]=port<256?2:1;assert(run(30));return session.native.gpr[0];}
static void pic_init(void){mock.f[0x6820]=0;assert(outp(0x20,17)&&outp(0x21,48)&&outp(0x21,4)&&outp(0x21,1)&&outp(0x21,0xea));mock.f[0x6820]=512;}
static void uart_thre(void){assert(outp(0x3fb,3)&&outp(0x3fc,8)&&outp(0x3f9,2));}
static void receipt(unsigned reason){struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=reason};assert(lgd_after_entry(&dispatch,&session,&r));}
static void pit_init(unsigned reload){assert(outp(0x43,0x34)&&outp(0x40,reload)&&outp(0x40,reload>>8));}
static bool reentrant_write(void*p,uint32_t f,uint64_t v){
 assert(lgd_send(&dispatch,'Q')==-16&&lgd_recv(&dispatch)==-16&&!lgd_reset(&dispatch)&&!lgd_before_entry(&dispatch,&session));
 return wr(p,f,v);
}
static bool safe(void*p){(void)p;return true;}
int main(void){
 old_regression();
 /* Pure RX capacity: no FIFO overflow/IRQ/clock mutation and no entry races. */
 setup04();struct lgd_context capacity_before=dispatch;assert(lgd_space(&dispatch)==1&&!memcmp(&dispatch,&capacity_before,sizeof dispatch));
 assert(lgd_send(&dispatch,'A')==0);capacity_before=dispatch;assert(lgd_space(&dispatch)==0&&!memcmp(&dispatch,&capacity_before,sizeof dispatch));
 assert(!dispatch.pic04.uart.uart.overrun);assert(inp(0x3f8)=='A');assert(outp(0x3fa,0x81));
 assert(outp(0x3fc,16));assert(lgd_space(&dispatch)==0);assert(outp(0x3fc,0));
 assert(lgd_space(&dispatch)==16);for(unsigned i=0;i<16;i++){assert(lgd_space(&dispatch)==(int)(16-i));assert(lgd_send(&dispatch,'B')==0);}
 capacity_before=dispatch;assert(lgd_space(&dispatch)==0&&!memcmp(&dispatch,&capacity_before,sizeof dispatch));
 dispatch.busy=true;assert(lgd_space(&dispatch)==-(int)SYLE_GUEST_EBUSY);dispatch.busy=false;
 dispatch.entry_inflight=true;assert(lgd_space(&dispatch)==-(int)SYLE_GUEST_EBUSY);dispatch.entry_inflight=false;
 dispatch.pic04.uart.uart.rc=17;capacity_before=dispatch;assert(lgd_space(&dispatch)==-(int)SYLE_GUEST_EBACKEND&&!memcmp(&dispatch,&capacity_before,sizeof dispatch));
 dispatch.failed=true;assert(lgd_space(&dispatch)==-(int)SYLE_GUEST_ESTATE);
 setup(false);assert(lgd_space(&dispatch)==-(int)SYLE_GUEST_ENOTSUP);

 setup04();pic_init();uart_thre();assert(dispatch.pic04.master.irr==16&&dispatch.pic04.master.level4&&!dispatch.pic04.uart.irq_dirty&&!dispatch.uart.ready);
 assert(lgd_before_entry(&dispatch,&session)&&dispatch.pic04.master.reserved.source==4&&mock.f[0x4016]==0x80000034);
 assert(lgd_send(&dispatch,'X')==-16&&lgd_recv(&dispatch)==-16);receipt(30);assert(dispatch.pic04.master.isr==16&&!dispatch.pic04.master.irr&&dispatch.pic04.master.completed.source==4);
 assert((inp(0x3fa)&15)==2&&!dispatch.pic04.master.level4);assert(outp(0x20,0x20)&&!dispatch.pic04.master.isr);
 assert(lgd_send(&dispatch,'R')==0);assert(inp(0x3f8)=='R');assert(!dispatch.uart.ready); /* new domain is the only active UART */
 /* Actual UART READ-CLEAR plus staged PIT/PIC commit retains low line. */
 assert(outp(0x3f9,0)&&outp(0x3f9,2)&&dispatch.pic04.master.irr==16);assert(inp(0x3fa)==2&&dispatch.pic04.master.irr==16&&!dispatch.pic04.master.level4);
 assert(lgd_before_entry(&dispatch,&session));receipt(30);assert(outp(0x20,0x64)&&!dispatch.pic04.master.isr);
 /* PIT0 source can preempt in-service lower-priority IRQ4, never vice versa. */
 assert(outp(0x3f9,0)&&outp(0x3f9,2));assert(lgd_before_entry(&dispatch,&session));receipt(30);assert(dispatch.pic04.master.isr==16);
 pit_init(2);assert(run(52)&&run(52)&&run(52));assert(dispatch.pic04.master.irr&1);
 assert(lgd_before_entry(&dispatch,&session)&&dispatch.pic04.master.reserved.source==0);receipt(30);assert(dispatch.pic04.master.isr==17);
 assert(outp(0x20,0x20)&&dispatch.pic04.master.isr==16);assert(outp(0x20,0x64)&&dispatch.pic04.master.isr==0);
 /* Definitive no-entry cancels only reservation, leaving pending edge. */
 setup04();pic_init();uart_thre();assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result no={.state_result=LVS_ENTRY_FAILED};assert(lgd_after_entry(&dispatch,&session,&no)&&dispatch.pic04.master.irr==16&&!dispatch.pic04.master.reserved.serial);
 /* Unknown post-entry receipt is quarantined, not fake success. */
 setup04();pic_init();uart_thre();assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result unknown={.state_result=LVS_EXIT,.fields_valid=true,.reason=52};assert(!lgd_after_entry(&dispatch,&session,&unknown)&&dispatch.failed&&dispatch.pic04.master.quarantined);
 /* Partial VMWRITE fails before committing UART/domain/GPR/time. */
 for(unsigned i=1;i<=4;i++){setup04();pic_init();struct lpd_domain prev=dispatch.pic04;uint64_t tick=dispatch.ticks;session.native.gpr[0]=3;session.native.gpr[3]=0x3fb;uint64_t regs[15];memcpy(regs,session.native.gpr,sizeof regs);mock.f[0x6400]=0x3fb0000;mock.f[0x440c]=1;mock.failwrite=mock.writes+i;assert(!run(30)&&dispatch.failed&&dispatch.ticks==tick&&!memcmp(&dispatch.pic04,&prev,sizeof prev)&&!memcmp(regs,session.native.gpr,sizeof regs));assert(!run(52));}
 /* Receive-full commits OE, does not duplicate a byte or lose the PIC update. */
 setup04();pic_init();assert(outp(0x3fb,3)&&outp(0x3fc,8)&&outp(0x3f9,5));assert(lgd_send(&dispatch,'A')==0&&lgd_send(&dispatch,'B')==-16&&dispatch.pic04.uart.uart.overrun&&dispatch.pic04.master.irr==16);assert(inp(0x3f8)=='A');
 /* A stable high edge already accepted and EOIed does not manufacture IRQ4,
  * nor block a future PIT deadline. New pending IRQ4 prevents the idle jump. */
 setup04();pic_init();uart_thre();assert(lgd_before_entry(&dispatch,&session));receipt(30);assert(outp(0x20,0x64));pit_init(4773);mock.f[0x4826]=1;uint64_t tick=dispatch.ticks;assert(run(52)&&dispatch.ticks>tick+1&&dispatch.pic04.master.irr==1&&dispatch.pic04_idle_reason==LPI_JUMP&&dispatch.pic04.master.level4);
 setup04();pic_init();pit_init(4773);uart_thre();mock.f[0x4826]=1;tick=dispatch.ticks;assert(run(52)&&dispatch.ticks==tick+1&&dispatch.pic04_idle_reason==LPI_PENDING);
 /* Configuration isolation: no two interrupt domains or foreign UART sink. */
 setup(false);struct lgd_config bad=dispatch.config;bad.legacy_pic04_enabled=true;struct lgd_context fresh={0};struct lgm_service_ops ops;assert(!lgd_init(&fresh,&bad,&ops));bad.sync_irq=0;bad.irq_ctx=0;bad.legacy_pic_enabled=true;assert(!lgd_init(&fresh,&bad,&ops));
 /* Full owned-event proof precedes the first window write. */
 for(unsigned k=0;k<5;k++){setup04();unsigned writes=mock.writes;if(k==0)mock.f[0x4016]=0x80000034;if(k==1)mock.f[0x4018]=1;if(k==2)mock.f[0x401a]=1;if(k==3)mock.f[0x4408]=0x80000034;if(k==4)dispatch.bp.pending=true;assert(!lgd_before_entry(&dispatch,&session)&&dispatch.failed&&mock.writes==writes&&!dispatch.pic04.master.reserved.serial);}
 /* Read failure never publishes time/source state. */
 for(unsigned k=1;k<=7;k++){setup04();pic_init();struct lpd_domain prev=dispatch.pic04;uint64_t tick=dispatch.ticks;mock.failread=mock.reads+k;assert(!outp(0x3fb,3)&&dispatch.failed&&dispatch.ticks==tick&&!memcmp(&prev,&dispatch.pic04,sizeof prev));}
 /* New source cannot be made routable by unmasking unsupported PIC pins. */
 setup04();pic_init();struct lpd_domain prev=dispatch.pic04;uint64_t ticks=dispatch.ticks;assert(!outp(0x21,0xe8)&&dispatch.failed&&dispatch.ticks==ticks&&!memcmp(&prev,&dispatch.pic04,sizeof prev));
 /* Full-domain copy needs callback rebinding; no copy targets live master. */
 setup04();struct lpd_domain clone=dispatch.pic04;assert(!lpd_valid(&clone));lpd_bind(&clone);assert(lpd_valid(&clone));
 /* The dispatcher, not an external callback, owns reentry exclusion. */
 setup04();pic_init();session.hop.vmwrite=reentrant_write;assert(outp(0x3fb,3)&&dispatch.pic04.uart.uart.lcr==3);session.hop.vmwrite=wr;
 /* Actual callback failure after staged UART change does not publish it. */
 setup04();pic_init();assert(outp(0x3fb,3)&&outp(0x3fc,8));dispatch.pic04.master.seen4=UINT64_MAX;prev=dispatch.pic04;ticks=dispatch.ticks;assert(!outp(0x3f9,2)&&dispatch.failed&&dispatch.ticks==ticks&&!memcmp(&prev,&dispatch.pic04,sizeof prev));
 /* External byte API failure likewise doesn't publish a byte/result. */
 setup04();pic_init();assert(outp(0x3fb,3)&&outp(0x3fc,8)&&outp(0x3f9,1));dispatch.pic04.master.seen4=UINT64_MAX;prev=dispatch.pic04;assert(lgd_send(&dispatch,'R')==-16&&dispatch.failed&&!memcmp(&prev,&dispatch.pic04,sizeof prev));
 /* Each entry staging and receipt cleanup write failure retains ownership
  * and fails closed, never accounting an uncertain IRQ4 delivery. */
 for(unsigned k=1;k<=3;k++){
  setup04();pic_init();uart_thre();mock.failwrite=mock.writes+k;assert(!lgd_before_entry(&dispatch,&session)&&dispatch.failed&&dispatch.pic04.master.reserved.serial&&dispatch.pic04_run.irq.failed&&dispatch.pic04_run.failed&&!dispatch.pic04.master.completed.serial);
  setup04();pic_init();uart_thre();assert(lgd_before_entry(&dispatch,&session));mock.failwrite=mock.writes+k;struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=30};assert(!lgd_after_entry(&dispatch,&session,&r)&&dispatch.failed&&dispatch.pic04.master.reserved.serial&&dispatch.pic04_run.irq.failed&&dispatch.pic04_run.failed&&!dispatch.pic04.master.completed.serial&&!dispatch.pic04.master.isr);
 }
 /* Window defer is a real control update; window exit never proves receipt. */
 setup04();pic_init();uart_thre();mock.f[0x6820]=0;assert(lgd_before_entry(&dispatch,&session)&&(mock.f[0x4002]&4)&&!dispatch.pic04.master.reserved.serial);receipt(7);mock.f[0x6820]=512;assert(run(7)&&!(mock.f[0x4002]&4)&&!dispatch.pic04.master.completed.serial);assert(lgd_before_entry(&dispatch,&session));receipt(30);
 /* Explicit standalone bridge source test, NOT a claim that runtime SEND is
  * permitted while a vCPU is in flight. A later edge survives old receipt. */
 setup04();pic_init();uart_thre();struct pic_master_04 master=dispatch.pic04.master;struct pmg gate;assert(pmg_init(&gate,&master));struct lei_snapshot guest={.rflags=512};assert(pmg_before(&gate,&master,&guest,&mock,wr)==PMG_READY);assert(pm04_level4(&master,master.domain,false)&&pm04_level4(&master,master.domain,true));assert(pmg_after(&gate,&master,PMG_EXIT,30,0,&mock,wr)&&master.irr==16&&master.isr==16);
 setup04();assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result uncertain={.state_result=LVS_GUEST_CAPTURE_FAILED};assert(!lgd_after_entry(&dispatch,&session,&uncertain)&&dispatch.failed); /* no ticket isn't proof of a safe outcome */
 /* Read-only progress has explicit source and once-only IRQ4 marker. */
 setup04();pic_init();uart_thre();assert(lgd_before_entry(&dispatch,&session));receipt(30);session.host.active=session.host.ready=true;session.platform.safe=safe;struct lprog_snapshot snap;struct lpd_domain saved=dispatch.pic04;assert(lprog_collect(&dispatch,&session,&snap)&&snap.receipt_source==4&&snap.irr==0&&snap.isr==16&&!memcmp(&saved,&dispatch.pic04,sizeof saved));bool sent=false;assert(!lprog_irq4_once(&sent,false)&&!sent);assert(lprog_irq4_once(&sent,true)&&sent);for(unsigned i=0;i<100;i++)assert(!lprog_irq4_once(&sent,true));
 /* Linux oneshot control0x38: count1 produces one IRQ0 and no periodic tail. */
 setup04();pic_init();assert(outp(0x43,0x38)&&outp(0x40,1)&&outp(0x40,0));
 assert(dispatch.pic04.timer.mode4&&dispatch.pic04.timer.reload==1);
 uint64_t deadline;assert(pit0_next_edge(&dispatch.pic04.timer,&deadline));
 mock.f[0x4826]=1;assert(run(52)&&dispatch.ticks==deadline&&dispatch.pic04.master.irr==1&&dispatch.pic04_idle_reason==LPI_JUMP);
 assert(lgd_before_entry(&dispatch,&session)&&dispatch.pic04.master.reserved.source==0);receipt(30);
 mock.f[0x4826]=0;assert(outp(0x20,0x20)&&dispatch.pic04.master.isr==0);
 mock.f[0x4826]=1;tick=dispatch.ticks;
 assert(run(52)&&dispatch.ticks==tick+1&&dispatch.pic04_idle_reason==LPI_NO_TIMER&&!dispatch.pic04.master.irr);
 for(unsigned i=0;i<20;i++)assert(run(52)&&dispatch.pic04.timer.edges==1&&!dispatch.pic04.master.irr);
 /* Control programming publishes no mode/edge/time changes on VMWRITE failure. */
 for(unsigned i=1;i<=4;i++){
  setup04();pic_init();saved=dispatch.pic04;tick=dispatch.ticks;mock.failwrite=mock.writes+i;
  assert(!outp(0x43,0x38)&&dispatch.failed&&dispatch.ticks==tick&&!memcmp(&saved,&dispatch.pic04,sizeof saved));
 }
 puts("PIC04 combination host PASS; no VM/IRQ hardware acceptance");
}
