#include "pic0.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void wr(struct pic0*s,unsigned p,unsigned b){uint8_t v=b;assert(pic0_access(s,p,true,&v));}
static uint8_t rd(struct pic0*s,unsigned p){uint8_t b=0;assert(pic0_access(s,p,false,&b));return b;}
static void ready(struct pic0*s,bool aeoi){assert(pic0_init(s,7,0));wr(s,0x20,0x11);assert(rd(s,0x21)==0);wr(s,0x21,0x30);wr(s,0x21,4);wr(s,0x21,aeoi?3:1);assert(rd(s,0x21)==0&&s->mask_pending);wr(s,0x21,0xfe);}
int main(void){struct pic0 s;uint64_t t;uint8_t v;
 ready(&s,false);assert(!pic0_peek(&s,&v));assert(pic0_edges(&s,7,100));assert(rd(&s,0x20)==1);assert(pic0_reserve(&s,&t,&v)&&v==0x30);assert(!pic0_access(&s,0x21,true,&v));assert(pic0_edges(&s,7,102));assert(pic0_accept(&s,7,t,0x30)&&s.irr&&s.isr);assert(!pic0_peek(&s,&v));assert(pic0_accept(&s,7,t,0x30)&&s.irr);wr(&s,0x20,0x0b);assert(rd(&s,0x20)==1);wr(&s,0x20,0x20);assert(!s.isr&&pic0_reserve(&s,&t,&v));assert(pic0_cancel(&s,7,t,v)&&s.irr&&!s.isr);assert(pic0_reserve(&s,&t,&v));assert(pic0_accept(&s,7,t,v)&&!s.irr&&s.isr);wr(&s,0x20,0x60);assert(!s.isr);
 ready(&s,true);assert(pic0_edges(&s,7,1)&&pic0_reserve(&s,&t,&v)&&pic0_accept(&s,7,t,v)&&!s.isr&&!s.irr);
 ready(&s,false);wr(&s,0x21,255);assert(pic0_edges(&s,7,1)&&s.irr&&!pic0_peek(&s,&v));wr(&s,0x21,254);assert(pic0_peek(&s,&v));
 struct pic0 old=s;assert(!pic0_edges(&s,8,2)&&!memcmp(&s,&old,sizeof s));assert(!pic0_edges(&s,7,0));uint8_t bad=0xfd;assert(!pic0_access(&s,0x21,true,&bad)&&!memcmp(&s,&old,sizeof s));bad=0xa0;assert(!pic0_access(&s,0x20,true,&bad));
 assert(pic0_reserve(&s,&t,&v));old=s;assert(!pic0_accept(&s,8,t,v)&&!memcmp(&s,&old,sizeof s));assert(!pic0_accept(&s,7,t+1,v));assert(!pic0_cancel(&s,7,t,v+1));assert(pic0_cancel(&s,7,t,v));s.serial=UINT64_MAX;old=s;assert(!pic0_reserve(&s,&t,&v)&&!memcmp(&s,&old,sizeof s));
 ready(&s,false);assert(pic0_edges(&s,7,5));wr(&s,0x20,0x11);assert(!s.irr&&!s.isr&&s.seen==5&&s.imr==0);assert(pic0_edges(&s,7,7)&&!s.irr);wr(&s,0x21,0x38);wr(&s,0x21,4);wr(&s,0x21,1);assert(!pic0_peek(&s,&v));wr(&s,0x21,254);assert(!s.irr);assert(pic0_edges(&s,7,8)&&s.irr);
 for(unsigned m=0;m<256;m++){ready(&s,false);uint8_t b=m;bool allowed=m==250||m==251||m==254||m==255;assert(pic0_access(&s,0x21,true,&b)==allowed);}
 ready(&s,false);assert(pic0_edges(&s,7,1)&&pic0_reserve(&s,&t,&v)&&pic0_accept(&s,7,t,v));uint64_t stale=t;wr(&s,0x20,0x11);wr(&s,0x21,0x30);wr(&s,0x21,4);wr(&s,0x21,1);wr(&s,0x21,254);assert(!pic0_accept(&s,7,stale,0x30));
 assert(pic0_init(&s,7,0));wr(&s,0x20,0x11);bad=0x31;old=s;assert(!pic0_access(&s,0x21,true,&bad)&&!memcmp(&s,&old,sizeof s));wr(&s,0x21,0x30);bad=2;assert(!pic0_access(&s,0x21,true,&bad));wr(&s,0x21,4);bad=9;assert(!pic0_access(&s,0x21,true,&bad));bad=1;assert(!pic0_access(&s,0xa1,true,&bad));
 puts("PIC0 pure host: actual ICW IMR0, IRQ0 edge latch/coalescing, ISR priority/EOI/AEOI, reservation new-edge preservation PASS; not wired");return 0;}
