#include "../firmware_timer_stage.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void write_ok(struct fts_state *s,uint64_t now,uint64_t off,uint32_t v) {
 assert(fts_mmio(s,7,now,off,4,true,&v)==FTS_OK);
}
int main(void) {
 struct fts_state s={0};assert(fts_init(&s,7,0));
 write_ok(&s,0,0x320,0x20005); /* inferred OVMF setup, not evidence */
 struct fts_state old=s;uint32_t v=0xffffffff;
 assert(fts_mmio(&s,7,0,0x380,4,true,&v)==FTS_STOP_VECTOR);
 assert(!memcmp(&s,&old,sizeof s));assert(v==0xffffffff);
 write_ok(&s,0,0x320,0x30005);write_ok(&s,0,0x380,2);
 assert(s.timer.deadline==4096);
 v=0;assert(fts_mmio(&s,7,2048,0x390,4,false,&v)==FTS_OK && v==1);
 assert(fts_poll(&s,7,12288)==FTS_OK && s.timer.deadline==16384 && s.timer.last_expirations==3);
 old=s;v=5;assert(fts_mmio(&s,7,12288,0x320,4,true,&v)==FTS_STOP_VECTOR);assert(!memcmp(&s,&old,sizeof s));
 write_ok(&s,12288,0x320,0x20040); /* preserve periodic phase */
 assert(s.timer.deadline==16384);
 old=s;assert(fts_poll(&s,7,16384)==FTS_STOP_IRQ);assert(!memcmp(&s,&old,sizeof s));
 v=0;assert(fts_mmio(&s,7,16384,0x380,4,true,&v)==FTS_STOP_IRQ);assert(!memcmp(&s,&old,sizeof s));
 v=0x30040;assert(fts_mmio(&s,7,16384,0x320,4,true,&v)==FTS_STOP_IRQ);assert(!memcmp(&s,&old,sizeof s));
 assert(fts_poll(&s,8,12288)==FTS_INVALID);assert(!memcmp(&s,&old,sizeof s));
 assert(fts_poll(&s,7,12287)==FTS_INVALID);assert(!memcmp(&s,&old,sizeof s));
 assert(fts_init(&s,7,0));write_ok(&s,0,0x380,1);assert(fts_poll(&s,7,2048)==FTS_OK && !s.timer.armed);
 old=s;v=1;assert(fts_mmio(&s,7,3000,0x999,4,true,&v)==FTS_INVALID);assert(!memcmp(&s,&old,sizeof s));
 assert(fts_init(&s,7,UINT64_MAX-10));old=s;v=1;assert(fts_mmio(&s,7,UINT64_MAX-10,0x380,4,true,&v)==FTS_INVALID);assert(!memcmp(&s,&old,sizeof s));
 /* All divider encodings, phase and pre-deadline mask ordering. */
 const unsigned enc[8]={0,1,2,3,8,9,10,11};
 const unsigned div[8]={2,4,8,16,32,64,128,1};
 for(unsigned i=0;i<8;i++) {
  assert(fts_init(&s,7,0));write_ok(&s,0,0x3e0,enc[i]);
  write_ok(&s,0,0x320,0x20040);write_ok(&s,0,0x380,3);
  uint64_t d=3*UINT64_C(1024)*div[i];assert(s.timer.deadline==d);
  write_ok(&s,d-1,0x320,0x30040);
  assert(fts_poll(&s,7,d*4)==FTS_OK && s.timer.last_expirations==4 && s.timer.deadline==d*5);
  write_ok(&s,d*4,0x320,0x20040);assert(s.timer.deadline==d*5);
  assert(fts_poll(&s,7,d*5)==FTS_STOP_IRQ);
 }
 assert(fts_init(&s,7,0));write_ok(&s,0,0x380,1);old=s;v=4;
 assert(fts_mmio(&s,7,0,0x3e0,4,true,&v)==FTS_INVALID);assert(!memcmp(&s,&old,sizeof s));
 /* Even a masked due expiry rolls back if the requested operation fails. */
 v=0xabcdef;assert(fts_mmio(&s,7,2048,0x999,4,false,&v)==FTS_INVALID);assert(v==0xabcdef);assert(!memcmp(&s,&old,sizeof s));
 puts("firmware_timer_stage: PASS (host-only, no IRQ sink)");
}
