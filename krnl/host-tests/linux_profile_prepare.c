#define main gate_regression
#include "linux_profile_gate.c"
#undef main
#include "../linux_profile_prepare.h"
static unsigned writes,fail_at;static struct lpp_context *reenter;
static bool wb(void*c,uint64_t pa){(void)c;(void)pa;writes++;if(reenter){assert(!lpp_prepare_guest(reenter,&s));}return writes!=fail_at;}
static void fresh(void){setup();s.enabled=s.prepared=s.host.ready=s.host.active=false;s.memory.owner.writeback=wb;put(ram+0x90070,0,8);writes=fail_at=0;reenter=NULL;}
int main(void){ram=calloc(1,LGM_RAM_SIZE);tables=calloc(LGM_TABLE_COUNT,4096);assert(ram&&tables);
 struct lpp_context*p=calloc(1,sizeof *p);assert(p);fresh();assert(lpp_init(p,456,NULL,now));reenter=p;
 assert(lpp_prepare_guest(p,&s)&&p->prepared&&p->firmware==LAI_INSTALLED&&writes==2&&s.memory.prepared&&!s.enabled);
 assert(!lpp_prepare_guest(p,&s));
 for(unsigned failure=1;failure<=2;failure++){fresh();memset(p,0,sizeof *p);assert(lpp_init(p,456+failure,NULL,now));fail_at=failure;
 assert(!lpp_prepare_guest(p,&s)&&p->failed&&p->firmware==LAI_DIRTY&&s.memory.prepared&&!s.enabled);
 fail_at=0;assert(!lpp_prepare_guest(p,&s));}
 fresh();memset(p,0,sizeof *p);assert(lpp_init(p,459,NULL,now));s.host.active=true;assert(!lpp_prepare_guest(p,&s)&&!writes&&p->failed);
 fresh();memset(p,0,sizeof *p);assert(lpp_init(p,460,NULL,now));s.memory.pages[0x90]=0;assert(!lpp_prepare_guest(p,&s)&&!writes&&p->failed);
 free(p);free(ram);free(tables);puts("linux_profile_prepare: owned install, dirty retention, no VMX/release, reentry and preconditions PASS (host only)");return 0;}
