#define main original_tests
#include "linux_guest_dispatch.c"
#undef main
static void gp_setup(void){setup(false);high();dispatch.pf=(struct lpfg_state){0};put(0x16f5000+511*8,0x7000063);put(0x7000000+510*8,0x7001063);put(0x7001000+11*8,0x16000e3);session.native.gpr[2]=0xce;mock.f[0x681e]=0xffffffff816cf508ull;mock.f[0x440c]=2;mock.f[0x4402]=31;mock.f[0x4404]=mock.f[0x4406]=0;put(0x16cf508,0x320f);put(0x174a0d0,0x816c8e000010f3deull);put(0x174a0d8,0xffffffff);}
int main(void){original_tests();gp_setup();uint64_t rip=mock.f[0x681e],cr2=session.native.cr2,regs[15];memcpy(regs,session.native.gpr,sizeof regs);assert(run(31)&&dispatch.msr_gp.pending&&dispatch.msr_gp.count==1&&mock.f[0x4016]==0x80000b0d&&!mock.f[0x4018]&&!mock.f[0x401a]&&mock.f[0x6820]==0x10000&&mock.f[0x681e]==rip&&session.native.cr2==cr2&&!memcmp(regs,session.native.gpr,sizeof regs));assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result receipt={.state_result=LVS_EXIT,.fields_valid=true,.reason=10};assert(lgd_after_entry(&dispatch,&session,&receipt));session.native.gpr[0]=1;assert(run(10)&&!dispatch.msr_gp.pending);free(ram);
 for(unsigned bad=0;bad<9;bad++){gp_setup();if(bad==0)put(0x16cf508,0x300f);if(bad==1)put(0x174a0d0,0x816c8f000010f3deull);if(bad==2)mock.f[0x681c]=0x10;if(bad==3)mock.f[0x440c]=3;if(bad==4)mock.f[0x6820]=0x100;if(bad==5)mock.f[0x6822]=1;if(bad==6)dispatch.msr_gp.count=LGP_MAX_FAULTS;if(bad==7){dispatch.msr_gp.count=1;dispatch.msr_gp.history[0]=(struct lgp_tuple){mock.f[0x681e],mock.f[0x6802]};}if(bad==8)put(0x7001000+11*8,0x16000a1);assert(!run(31)&&dispatch.failed);free(ram);}
 for(unsigned fail=4;fail<=7;fail++){gp_setup();mock.failwrite=fail;assert(!run(31)&&dispatch.failed&&!dispatch.msr_gp.count);assert(!run(31));free(ram);}
 gp_setup();assert(run(31));assert(!run(52)&&dispatch.msr_gp.pending&&dispatch.failed);free(ram);
 gp_setup();assert(run(31));mock.f[0x4018]=2;assert(!lgd_before_entry(&dispatch,&session)&&dispatch.failed);free(ram);
 for(unsigned f=1;f<=35;f++){gp_setup();mock.failread=f;bool ok=run(31);if(!ok)assert(dispatch.failed&&!dispatch.msr_gp.count);free(ram);}
 gp_setup();assert(run(31));dispatch.pf=(struct lpfg_state){1,1,true,false};unsigned writes=mock.writes;assert(!lgd_before_entry(&dispatch,&session)&&mock.writes==writes);free(ram);
 gp_setup();assert(run(31));mock.f[0x4016]=0x80000b0e;assert(!lgd_before_entry(&dispatch,&session));free(ram);
 gp_setup();mock.f[0x4016]=0x80000030;assert(!run(31)&&dispatch.failed);free(ram);
 gp_setup();session.native.gpr[2]=0xce;assert(!run(32)&&dispatch.failed);free(ram);
 gp_setup();assert(run(31));mock.f[0x6820]=0;assert(!lgd_before_entry(&dispatch,&session)&&dispatch.failed);free(ram);
 gp_setup();dispatch.config.legacy_pic_enabled=true;session.host.caps.primary=UINT64_C(0xffffffff00000000);mock.f[0x4002]=0x80000080;assert(lgd_before_entry(&dispatch,&session));struct lgs_entry_result noentry={.state_result=LVS_ENTRY_FAILED};assert(lgd_after_entry(&dispatch,&session,&noentry));assert(run(31));mock.f[0x4018]=2;assert(!lgd_before_entry(&dispatch,&session)&&dispatch.failed);free(ram);
 puts("strict MSR GP host PASS");}
