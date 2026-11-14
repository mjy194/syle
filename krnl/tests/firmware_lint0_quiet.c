#include "../firmware_lint0_quiet.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void check(struct lpic_state*s,bool expected){struct lpic_state saved=*s;assert(flq_pic_quiet(s)==expected&&!memcmp(s,&saved,sizeof saved));}
int main(void){
 assert(!flq_pic_quiet(0));struct lpic_state s;lpic_init(&s);check(&s,true);s.master=0xfb;check(&s,true);
 for(unsigned m=0;m<256;m++)for(unsigned n=0;n<256;n++){lpic_init(&s);s.master=m;s.slave=n;check(&s,(m==0xff||m==0xfb)&&n==0xff);}
 lpic_init(&s);s.failed=true;check(&s,false);
 for(unsigned chip=0;chip<2;chip++)for(unsigned phase=1;phase<256;phase++){lpic_init(&s);s.chip[chip].phase=phase;check(&s,false);}
 /* Legitimate in-progress ICW is admitted with IF0 by the PIC model but NOT
  * by this predicate, even though caller's actual RFLAGS might have IF0. */
 for(unsigned chip=0;chip<2;chip++)for(unsigned phase=1;phase<=3;phase++){
  lpic_init(&s);s.chip[chip].started=true;s.chip[chip].phase=phase;
  if(chip)s.slave=0;else s.master=0;
  if(phase>=2)s.chip[chip].vector=chip?0x28:0x20;
  if(phase>=3)s.chip[chip].cascade=chip?2:4;
  assert(lpic_entry_ok(&s,0));check(&s,false);
 }
 /* Completed remapped chips must also satisfy lpic's internal shape rules. */
 lpic_init(&s);for(unsigned i=0;i<2;i++)s.chip[i]=(struct lpic_chip){.started=true,.vector=i?0x28:0x20,.cascade=i?2:4};check(&s,true);
 s.master=0xfb;check(&s,true);s.chip[1].aeoi=true;check(&s,true);
 for(unsigned i=0;i<2;i++){struct lpic_state bad=s;bad.chip[i].vector=1;check(&bad,false);bad=s;bad.chip[i].cascade=0;check(&bad,false);bad=s;bad.chip[i].started=false;check(&bad,false);}
 puts("LINT0 PIC quiet: IF1 admission, exhaustive masks, ICW/invariant rejection, nonmutating PASS");return 0;
}
