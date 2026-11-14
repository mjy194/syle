#include "firmware_lint0_quiet.h"
bool flq_pic_quiet(const struct lpic_state*s){
 return s&&!s->failed&&(s->master==0xff||s->master==0xfb)&&s->slave==0xff&&
 !s->chip[0].phase&&!s->chip[1].phase&&lpic_entry_ok(s,UINT64_C(1)<<9);
}
