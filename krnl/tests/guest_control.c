#include "../guest_control.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define E(x) (-(int64_t)SYLE_GUEST_##x)
struct fake { struct guest_control *c; int fail_prepare,fail_start,fail_stop,fail_release; unsigned allocated, running, calls; char trace[128]; bool reenter; };
static void event(struct fake *f,char v) { assert(f->calls+1<sizeof f->trace); f->trace[f->calls++]=v; f->trace[f->calls]=0; }
static int prepare(void *ctx,uint64_t profile) { struct fake *f=ctx; event(f,'p'); assert(profile==1); assert(!f->allocated); f->allocated=1;
 if(f->reenter) assert(guest_control_call(f->c,7,SYLE_GUEST_CREATE,0,1)==E(EBUSY));
 return f->fail_prepare; }
static int start(void *ctx) { struct fake *f=ctx; event(f,'s'); assert(f->allocated); f->running=1; return f->fail_start; }
static int stop(void *ctx) { struct fake *f=ctx; event(f,'t'); if(f->fail_stop) return 1; f->running=0; return 0; }
static int release(void *ctx) { struct fake *f=ctx; event(f,'r'); assert(!f->running); if(f->fail_release) return 1; f->allocated=0; return 0; }
static int queued=-1;
static int send_byte(void *ctx,uint8_t byte){(void)ctx;if(queued!=-1)return E(EBUSY);queued=byte;return 0;}
static int recv_byte(void *ctx){(void)ctx;if(queued==-1)return E(EBUSY);int b=queued;queued=-1;return b;}
static const struct guest_backend ops={.prepare=prepare,.start=start,.stop=stop,.release=release};
static void init(struct guest_control *c,struct fake *f) { memset(f,0,sizeof *f);f->c=c;guest_control_init(c,7,&ops,f); }
static int64_t call(struct guest_control*c,uint64_t op,uint64_t h,uint64_t arg) { return guest_control_call(c,7,op,h,arg); }
int main(void) {
 struct guest_control c;struct fake f;init(&c,&f);
 assert(guest_control_call(&c,0,SYLE_GUEST_CREATE,0,1)==E(EPERM));
 assert(guest_control_call(&c,8,SYLE_GUEST_CREATE,0,1)==E(EPERM));assert(f.calls==0);
 assert(call(&c,999,0,0)==E(EINVAL)); assert(call(&c,SYLE_GUEST_CREATE,0,2)==E(EINVAL));
 assert(call(&c,SYLE_GUEST_CREATE,1,1)==E(EINVAL));
 f.reenter=true;int64_t h=call(&c,SYLE_GUEST_CREATE,0,1);assert(h==1);
 assert(call(&c,SYLE_GUEST_CREATE,0,1)==E(EBUSY));assert(call(&c,SYLE_GUEST_STOP,h,0)==E(ESTATE));
 assert(call(&c,SYLE_GUEST_START,h,1)==E(EINVAL));assert(call(&c,SYLE_GUEST_START,h+1,0)==E(ENOENT));
 assert(call(&c,SYLE_GUEST_STATUS,h,0)==SYLE_GUEST_READY);
 assert(call(&c,SYLE_GUEST_CONSOLE_SEND,h,256)==E(EINVAL));
 assert(call(&c,SYLE_GUEST_CONSOLE_SEND,h,65)==E(ENOTSUP));
 assert(call(&c,SYLE_GUEST_CONSOLE_RECV,h,0)==E(ENOTSUP));
 assert(call(&c,SYLE_GUEST_START,h,0)==0);assert(call(&c,SYLE_GUEST_START,h,0)==E(ESTATE));
 assert(call(&c,SYLE_GUEST_STOP,h,0)==0);assert(call(&c,SYLE_GUEST_STOP,h,0)==0);
 assert(call(&c,SYLE_GUEST_START,h,0)==0);assert(call(&c,SYLE_GUEST_DESTROY,h,0)==0);
 assert(!strcmp(f.trace,"pststr"));assert(!f.allocated);
 assert(call(&c,SYLE_GUEST_STATUS,h,0)==E(ENOENT));assert(call(&c,SYLE_GUEST_CREATE,0,1)==2);
 assert(call(&c,SYLE_GUEST_START,h,0)==E(ENOENT));assert(guest_control_owner_exit(&c,8)==E(EPERM));
 assert(guest_control_owner_exit(&c,7)==0);assert(!f.allocated);
 init(&c,&f);h=call(&c,SYLE_GUEST_CREATE,0,1);
 assert(call(&c,SYLE_GUEST_STEP,h,0)==E(ESTATE));
 assert(call(&c,SYLE_GUEST_START,h,0)==0);assert(call(&c,SYLE_GUEST_STEP,h,0)==0);
 f.fail_stop=1;assert(guest_control_owner_exit(&c,7)==E(EQUARANTINED));assert(c.manager==7&&f.allocated);
 f.fail_stop=0;assert(guest_control_owner_exit(&c,7)==0);assert(!f.allocated&&c.manager==0);
 assert(call(&c,SYLE_GUEST_CREATE,0,1)==E(EPERM));
 init(&c,&f);c.backend.console_send=send_byte;c.backend.console_recv=recv_byte;h=call(&c,SYLE_GUEST_CREATE,0,1);
 assert(call(&c,SYLE_GUEST_CONSOLE_RECV,h,0)==E(EBUSY));
 assert(call(&c,SYLE_GUEST_CONSOLE_SEND,h,255)==0);assert(call(&c,SYLE_GUEST_CONSOLE_SEND,h,1)==E(EBUSY));
 assert(call(&c,SYLE_GUEST_CONSOLE_RECV,h,0)==255);assert(call(&c,SYLE_GUEST_CONSOLE_RECV,h,1)==E(EINVAL));
 assert(guest_control_call(&c,8,SYLE_GUEST_CONSOLE_SEND,h,2)==E(EPERM));assert(guest_control_owner_exit(&c,7)==0);
 /* Partial prepare rolls back; failed cleanup holds allocation and blocks reuse. */
 init(&c,&f);f.fail_prepare=1;assert(call(&c,SYLE_GUEST_CREATE,0,1)==E(EBACKEND));assert(!strcmp(f.trace,"ptr"));assert(!f.allocated);
 f.fail_stop=1;assert(call(&c,SYLE_GUEST_CREATE,0,1)==E(EQUARANTINED));assert(f.allocated);assert(c.state==SYLE_GUEST_QUARANTINED);
 assert(call(&c,SYLE_GUEST_CREATE,0,1)==E(EBUSY));assert(call(&c,SYLE_GUEST_REAP,1,0)==E(EINVAL));
 assert(call(&c,SYLE_GUEST_REAP,0,0)==E(EQUARANTINED));f.fail_stop=0;assert(call(&c,SYLE_GUEST_REAP,0,0)==0);
 assert(call(&c,SYLE_GUEST_REAP,0,0)==0);
 /* start can have partially entered: never free without successful stop. */
 init(&c,&f);h=call(&c,SYLE_GUEST_CREATE,0,1);f.fail_start=1;assert(call(&c,SYLE_GUEST_START,h,0)==E(EBACKEND));
 assert(call(&c,SYLE_GUEST_START,h,0)==E(EQUARANTINED));f.fail_stop=1;
 assert(call(&c,SYLE_GUEST_DESTROY,h,0)==E(EQUARANTINED));assert(f.allocated&&f.running);
 f.fail_stop=0;f.fail_release=1;assert(call(&c,SYLE_GUEST_DESTROY,h,0)==E(EQUARANTINED));assert(f.allocated&&!f.running);
 f.fail_release=0;assert(call(&c,SYLE_GUEST_DESTROY,h,0)==0);assert(!f.allocated);
 init(&c,&f);h=call(&c,SYLE_GUEST_CREATE,0,1);assert(call(&c,SYLE_GUEST_START,h,0)==0);assert(guest_control_owner_exit(&c,7)==0);assert(!strcmp(f.trace,"pstr"));
 init(&c,&f);c.generation=INT64_MAX;assert(call(&c,SYLE_GUEST_CREATE,0,1)==E(EEXHAUSTED));assert(f.calls==0);
 guest_control_init(&c,7,0,0);assert(call(&c,SYLE_GUEST_CREATE,0,1)==E(ENOTSUP));
 struct guest_backend broken=ops;broken.release=0;guest_control_init(&c,7,&broken,&f);assert(call(&c,SYLE_GUEST_CREATE,0,1)==E(ENOTSUP));
 puts("guest_control: parameter/owner/handle/state/rollback/quarantine/reentry/exit tests PASS");return 0;
}
