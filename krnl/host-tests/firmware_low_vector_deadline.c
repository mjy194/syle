/* Host-only semantic experiment: NOT an initialization API or native clock. */
#include "../firmware_timer_session.h"
#include "../linux_host_clock.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
 /* Verify the actual observed count has a representable synthetic deadline
  * in the existing model, without claiming this tick unit is calibrated. */
 struct fts_session s={0};assert(ftss_init(&s,99,0));
 uint32_t v=0x30005;
 assert(ftss_mmio(&s,99,0,0x320,4,true,&v)==FTS_OK);
 v=0xffffffff;assert(ftss_mmio(&s,99,0,0x380,4,true,&v)==FTS_OK);
 uint64_t due=UINT64_C(0xffffffff)*1024*2;
 assert(s.device.timer.deadline==due);
 /* Hypothetical future admitted unmasked low-vector running state. Direct
  * fixture assignment is TEST ONLY, bypasses current production admission. */
 s.device.timer.lvt=0x20005;
 assert(ftss_poll(&s,99,due-1)==FTS_OK);
 struct fts_state original=s.device;
 assert(ftss_poll(&s,99,due)==FTS_STOP_IRQ && s.terminal);
 assert(!memcmp(&s.device,&original,sizeof original));
 assert(ftss_poll(&s,99,due+100)==FTS_STOP_IRQ);
 /* IF is intentionally absent: it cannot suppress timer event ownership. */
 struct lhc_source src={.hz=10000000,.epoch=17,.cpu=0,.verified=true,.invariant=true};
 struct lhc_sample zero={.tsc=100,.epoch=17,.cpu=0};struct lhc_clock c={0};
 assert(lhc_init(&c,&src,zero,10000000));assert(lhc_start(&c,zero));
 struct lhc_clock before=c;struct lhc_sample bad=zero;bad.epoch++;
 assert(!lhc_advance(&c,bad)&&!memcmp(&c,&before,sizeof c));
 bad=zero;bad.cpu++;assert(!lhc_advance(&c,bad)&&!memcmp(&c,&before,sizeof c));
 bad=zero;bad.tsc--;assert(!lhc_advance(&c,bad)&&!memcmp(&c,&before,sizeof c));
 bad=zero;bad.tsc+=10000001;assert(!lhc_advance(&c,bad)&&!memcmp(&c,&before,sizeof c));
 src.verified=false;struct lhc_clock untrusted={0};assert(!lhc_init(&untrusted,&src,zero,10000000));
 puts("firmware_low_vector_deadline: PASS (synthetic fixture, no native admission)");
}
