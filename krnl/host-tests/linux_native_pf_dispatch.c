#define main diagnostic_fixture_tests
#include "linux_guest_dispatch.c"
#undef main
int main(void){
 setup(LVP_DIAGNOSTIC_PF);assert(!dispatch.pf.count&&!dispatch.demand_pf.count&&dispatch.high==LHP_UNUSED);
 assert(lgd_before_entry(&dispatch,&session));
 struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=52};
 assert(lgd_after_entry(&dispatch,&session,&r)&&run(52));
 mock.f[0x0802]=0x33;session.native.gpr[0]=1;assert(run(10));assert(run(16));
 assert(!dispatch.pf.count&&!dispatch.demand_pf.count&&dispatch.high==LHP_UNUSED);
 /* Unexpected intercepted PF never invokes the diagnostic injector. */
 setup(LVP_DIAGNOSTIC_PF);compressed();assert(!run(0)&&dispatch.failed&&dispatch.diagnostic_stage==LGD_STAGE_PF);
 assert(!dispatch.pf.pending&&!dispatch.pf.count&&!dispatch.demand_pf.count&&!session.native.cr2);
 assert(!run(52));
 puts("native PF dispatcher: fresh zero-PF state, user exits, unexpected PF fail-stop PASS (host only)");
 return 0;
}
