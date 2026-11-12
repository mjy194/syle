#define main original_tests
#include "linux_guest_dispatch.c"
#undef main
static bool post(unsigned v){session.native.gpr[0]=0x123400|v;mock.f[0x6400]=0x800040;mock.f[0x440c]=2;return run(30);}
int main(void){original_tests();for(unsigned legacy=0;legacy<2;legacy++){
 setup(false);if(legacy){dispatch.config.legacy_pic_enabled=true;session.host.caps.primary=UINT64_C(0xffffffff00000000);mock.f[0x4002]=0x80000080;assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result r={.state_result=LVS_EXIT,.fields_valid=true,.reason=52};assert(lgd_after_entry(&dispatch,&session,&r));}
 assert(post(255)&&dispatch.post.last==255&&dispatch.post.writes==1&&dispatch.ticks==1&&session.native.gpr[0]==0x1234ff);if(legacy)assert(dispatch.legacy.timer.timer.tick==1);
 session.prepared=false;assert(lgd_reset(&dispatch)&&!dispatch.post.writes&&!dispatch.post.last&&!dispatch.post.failed);
 }
 setup(false);mock.failwrite=4;assert(!post(7)&&dispatch.failed&&!dispatch.post.writes&&!dispatch.ticks);assert(!post(7));
 setup(false);dispatch.post.writes=UINT64_MAX;assert(!post(7)&&dispatch.failed&&!dispatch.ticks);
 setup(false);session.native.gpr[0]=1;mock.f[0x6400]=0x800048;assert(!run(30)&&dispatch.failed);
 puts("port80 dispatcher PASS");}
