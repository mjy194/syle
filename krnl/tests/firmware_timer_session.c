#include "../firmware_timer_session.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
 struct fts_session s={0};assert(ftss_init(&s,7,0));
 uint32_t v=0x40;assert(ftss_mmio(&s,7,0,0x320,4,true,&v)==FTS_OK);
 v=1;assert(ftss_mmio(&s,7,0,0x380,4,true,&v)==FTS_OK);
 struct fts_state old=s.device;
 assert(ftss_poll(&s,7,2048)==FTS_STOP_IRQ && s.terminal);
 assert(!memcmp(&old,&s.device,sizeof old));
 v=0;assert(ftss_mmio(&s,7,0,0x380,4,true,&v)==FTS_STOP_IRQ);
 assert(!ftss_init(&s,7,0));assert(!memcmp(&old,&s.device,sizeof old));
 ftss_abort(&s);assert(ftss_poll(&s,7,4096)==FTS_STOP_IRQ);
 struct fts_session t={0};assert(ftss_init(&t,7,0));
 v=99;assert(ftss_mmio(&t,7,100,0x999,4,false,&v)==FTS_INVALID && v==99 && !t.terminal);
 assert(ftss_poll(&t,7,99)==FTS_INVALID && t.terminal);
 assert(!ftss_init(&t,7,0));
 struct fts_session u={0};assert(ftss_init(&u,7,0));
 assert(ftss_poll(&u,8,0)==FTS_INVALID && u.terminal);
 struct fts_session f={0};assert(ftss_init(&f,7,0));ftss_abort(&f);
 assert(ftss_poll(&f,7,0)==FTS_INVALID && !ftss_init(&f,7,0));
 struct fts_session low={0};assert(ftss_init(&low,7,0));
 v=5;assert(ftss_mmio(&low,7,0,0x320,4,true,&v)==FTS_OK);
 v=1;assert(ftss_mmio(&low,7,0,0x380,4,true,&v)==FTS_STOP_VECTOR && low.terminal);
 for(unsigned bad=0;bad<3;bad++) {
  struct fts_session x={0};assert(ftss_init(&x,7,0));
  v=0x40;assert(ftss_mmio(&x,7,0,0x320,4,true,&v)==FTS_OK);
  v=1;assert(ftss_mmio(&x,7,0,0x380,4,true,&v)==FTS_OK);
  old=x.device;v=0xabcdef;
  assert(ftss_mmio(&x,7,2048,bad==2?0x999:0x320,bad==0?1:4,false,bad==1?0:&v)==FTS_STOP_IRQ);
  assert(x.terminal && v==0xabcdef && !memcmp(&old,&x.device,sizeof old));
 }
 puts("firmware_timer_session: PASS (terminal ownership, no delivery)");
}
