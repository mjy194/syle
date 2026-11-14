#define main legacy_fixture_main
#include "linux_pic04_fixture.c"
#undef main
#ifdef MG_HOST_CLOCK_DIAG
void log_text(const char*s){assert(s);}
void log_hex(uint64_t x){(void)x;}
#endif
static uint64_t host_now;static bool source_fail;static unsigned samples;
static bool sample_clock(void*p,struct lhc_sample*out){(void)p;++samples;if(source_fail)return false;*out=(struct lhc_sample){host_now,1,0};return true;}
static struct lhcr_source source={{10000000,1,0,true,true},600000000,0,sample_clock};
static bool port(unsigned p,uint8_t v){session.native.gpr[0]=v;session.native.gpr[3]=p;mock.f[0x6400]=((uint64_t)p<<16)|(p<256?64:0);mock.f[0x440c]=p<256?2:1;return run(30);}
static void setup_clock(void){
 setup(false);host_now=0;source_fail=false;samples=0;
 struct lgd_config c=dispatch.config;c.legacy_pic04_enabled=true;c.sync_irq=0;c.irq_ctx=0;c.host_clock=&source;c.max_exits=10000;
 dispatch=(struct lgd_context){0};struct lgm_service_ops ops;assert(lgd_init(&dispatch,&c,&ops));assert(ops.clock_start&&ops.clock_stop);
 assert(ops.clock_start(&dispatch));session.host.caps.primary=UINT64_C(0xffffffff00000000);mock.f[0x4002]=0x80000080;
 assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result r={LVS_EXIT,true,52,0};assert(lgd_after_entry(&dispatch,&session,&r));
}
int main(void){
 setup_clock();host_now=10000000;assert(run(52));assert(dispatch.ticks==1193182&&dispatch.pic04.timer.tick==1193182);
 assert(run(52)&&dispatch.ticks==1193182); /* repeated exit does not invent time */
 unsigned count=samples;assert(lgd_clock_start(&dispatch)&&samples==count); /* STEP */
 assert(lgd_clock_stop(&dispatch));uint64_t stopped=dispatch.ticks;host_now+=50000000;
 assert(lgd_clock_start(&dispatch)&&dispatch.ticks==stopped);host_now+=10000000;assert(run(52)&&dispatch.ticks==stopped+1193182);
 /* Time before arrival committed before RX age resets. */
 setup_clock();host_now=10000000;assert(lgd_send(&dispatch,'A')==0&&dispatch.ticks==1193182&&dispatch.pic04.uart.uart.rx_idle==0);
 /* Reserved ticket: external SEND rejected without even reading source. */
 setup_clock();dispatch.pic04.master.reserved.serial=1;count=samples;struct lgd_context old=dispatch;assert(lgd_send(&dispatch,'X')==-16&&samples==count&&!memcmp(&old,&dispatch,sizeof old));
 /* STOP cannot sync foreign reservation: fail-stop, but teardown retry works. */
 assert(!lgd_clock_stop(&dispatch)&&dispatch.failed);assert(lgd_clock_stop(&dispatch)&&!dispatch.host_clock.running);
 /* Source error and emulation failure publish neither clock nor device state. */
 setup_clock();source_fail=true;old=dispatch;assert(!run(52)&&dispatch.failed&&dispatch.ticks==old.ticks&&!memcmp(&dispatch.host_clock,&old.host_clock,sizeof old.host_clock));assert(lgd_clock_stop(&dispatch));
 setup_clock();host_now=10000000;mock.failread=mock.reads+1;old=dispatch;assert(!run(52)&&dispatch.failed&&dispatch.ticks==old.ticks&&!memcmp(&dispatch.host_clock,&old.host_clock,sizeof old.host_clock)&&!memcmp(&dispatch.pic04,&old.pic04,sizeof old.pic04));
 setup_clock();host_now=10000000;mock.failwrite=mock.writes+1;session.native.gpr[0]=3;session.native.gpr[3]=0x3fb;mock.f[0x6400]=0x3fb0000;mock.f[0x440c]=1;old=dispatch;assert(!run(30)&&dispatch.failed&&dispatch.ticks==old.ticks&&!memcmp(&dispatch.host_clock,&old.host_clock,sizeof old.host_clock));
 /* Missing source or incompatible idle model is not silently selected. */
 setup_clock();struct lgd_config bad=dispatch.config;bad.legacy_idle_advance=true;struct lgd_context fresh={0};struct lgm_service_ops ops;assert(!lgd_init(&fresh,&bad,&ops));
 #ifdef MG_HOST_CLOCK_DIAG
 setup_clock();assert(port(0x20,17)&&port(0x21,48)&&port(0x21,4)&&port(0x21,1)&&port(0x21,0xea));mock.f[0x6820]=512;
 assert(port(0x43,0x34)&&port(0x40,4773&255)&&port(0x40,4773>>8));
 host_now=50000;assert(run(52));assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result r={LVS_EXIT,true,30,0};assert(lgd_after_entry(&dispatch,&session,&r));assert(dispatch.clock_irq0_count==1);
 assert(lgd_clock_stop(&dispatch));
#else
 (void)port;
#endif
 #ifdef MG_HOST_CLOCK_TSC_FREQ
 setup(false);session.native.gpr[0]=0;mock.f[0x440c]=2;assert(run(10)&&session.native.gpr[0]==1);
 setup_clock();host_now=10000000;session.native.gpr[0]=0x15;mock.f[0x440c]=2;assert(run(10));assert(session.native.gpr[0]==1000&&session.native.gpr[1]==1193182&&session.native.gpr[2]==1000000);
 assert(run(16));uint64_t stamp=(uint32_t)session.native.gpr[0]|((uint64_t)(uint32_t)session.native.gpr[3]<<32);assert(stamp==1193182000);
 assert(lgd_clock_stop(&dispatch));host_now+=50000000;assert(lgd_clock_start(&dispatch));assert(run(16));assert(((uint32_t)session.native.gpr[0]|((uint64_t)(uint32_t)session.native.gpr[3]<<32))==stamp);
 host_now+=10000000;assert(run(16));assert(((uint32_t)session.native.gpr[0]|((uint64_t)(uint32_t)session.native.gpr[3]<<32))==stamp*2);
#endif
 puts("host-clock dispatch STEP/pause/RX ordering/IRQ busy/atomic failures PASS");return 0;
}
