#define main gate_regression
#include "host-tests/linux_profile_gate.c"
#undef main
#include "linux_profile_service.h"
static struct lgm_service_ops *live_ops;
static bool service_time(void*c,uint64_t*t){if(live_ops){assert(!live_ops->reset(live_ops->ctx));assert(live_ops->send(live_ops->ctx,1)==-16);}return now(c,t);}
static bool wb(void*c,uint64_t a){(void)c;(void)a;return true;}
static bool write_live(void*c,uint32_t n,uint64_t v){(void)c;f[n]=v;return true;}
static bool inv(void*c,uint64_t v){(void)c;(void)v;return true;}
int main(void){
 ram=calloc(1,LGM_RAM_SIZE);tables=calloc(LGM_TABLE_COUNT,4096);assert(ram&&tables);setup();
 struct lps_context*p=calloc(1,sizeof *p);assert(p);
 struct lgd_config c={.cpu=host,.max_basic=1,.vendor_b=0x756e6547,.vendor_c=0x6c65746e,.vendor_d=0x49656e69,.max_extended=0x80000008,.max_exits=100};
 struct lgm_service_ops ops;assert(lps_init(p,&c,0,service_time,&ops)&&p->generation==1&&ops.prepare_guest);
 live_ops=&ops;assert(ops.reset(ops.ctx)&&p->generation==2);
 put(ram+0x90070,0,8);s.prepared=s.enabled=s.host.active=s.host.ready=false;s.memory.owner.writeback=wb;
 assert(ops.prepare_guest(ops.ctx,&s)&&p->prepare.firmware==LAI_INSTALLED);
 assert(!ops.reset(ops.ctx)&&p->generation==2);
 s.prepared=s.enabled=s.host.active=s.host.ready=true;s.hop.vmwrite=s.host.owner.vmwrite=write_live;s.hop.invept=inv;
 assert(ops.before_entry(ops.ctx,&s)&&p->dispatch.activation.active);
 struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=10};assert(ops.after_entry(ops.ctx,&s,&r));
 f[0x4402]=10;f[0x440c]=2;s.native.gpr[0]=1;assert(ops.dispatch(ops.ctx,&s,10)&&(s.native.gpr[3]&(1u<<9)));
 s.prepared=s.enabled=s.host.active=s.host.ready=s.memory.prepared=false;
 /* Partly freed page token blocks reset even if flags falsely look quiescent. */
 assert(!ops.reset(ops.ctx));memset(s.memory.pages,0,sizeof s.memory.pages);memset(s.memory.tables,0,sizeof s.memory.tables);
 assert(ops.reset(ops.ctx)&&p->generation==3&&!p->dispatch.activation.active&&p->prepare.irq.domain==3);
 clockfail=true;assert(!ops.reset(ops.ctx)&&p->failed&&p->generation==4);
 clockfail=false;assert(ops.reset(ops.ctx)&&!p->failed&&p->generation==5);
 p->generation=UINT64_MAX;assert(!ops.reset(ops.ctx)&&p->generation==UINT64_MAX);
 free(p);free(ram);free(tables);puts("linux_profile_service: actual callback composition/create domain/prepare/activation/partial teardown/overflow PASS (host only)");return 0;
}
