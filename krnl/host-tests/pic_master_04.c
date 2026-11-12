#include "pic_master_04.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void wr(struct pic_master_04*s,unsigned p,unsigned b){uint8_t v=b;assert(pm04_access(s,p,true,&v,true));}
static void ready(struct pic_master_04*s,bool aeoi){assert(pm04_init(s,7,0,false));wr(s,0x20,17);wr(s,0x21,48);wr(s,0x21,4);wr(s,0x21,aeoi?3:1);wr(s,0x21,0xea);}
int main(void){
 struct pic_master_04 s,old;struct pm04_ticket a,b;uint8_t n,v;
 ready(&s,false);assert(pm04_level4(&s,7,true)&&s.irr==16);assert(pm04_reserve(&s,&a)&&a.source==4&&a.vector==52);
 assert(pm04_edges0(&s,7,1));assert(pm04_accept(&s,&a)&&s.isr==16&&s.irr==1);
 assert(pm04_reserve(&s,&b)&&b.source==0&&pm04_accept(&s,&b)&&s.isr==17);
 assert(pm04_level4(&s,7,false)&&pm04_level4(&s,7,true)&&s.irr==16);
 assert(!pm04_peek(&s,&n,&v));wr(&s,0x20,32);assert(s.isr==16&&!pm04_peek(&s,&n,&v));wr(&s,0x20,32);assert(!s.isr&&pm04_peek(&s,&n,&v)&&n==4);
 assert(pm04_reserve(&s,&a));assert(pm04_level4(&s,7,false)&&pm04_level4(&s,7,true));assert(pm04_accept(&s,&a)&&s.irr==16&&s.isr==16);
 assert(pm04_accept(&s,&a)&&s.irr==16);wr(&s,0x20,100);
 assert(pm04_reserve(&s,&a)&&pm04_cancel(&s,&a)&&s.irr==16);
 assert(pm04_reserve(&s,&a));old=s;b=a;b.source=0;assert(!pm04_accept(&s,&b)&&!memcmp(&s,&old,sizeof s));b=a;b.domain++;assert(!pm04_cancel(&s,&b));b=a;b.generation++;assert(!pm04_accept(&s,&b));b=a;b.vector++;assert(!pm04_accept(&s,&b));
 assert(pm04_quarantine(&s,&a));old=s;assert(!pm04_cancel(&s,&a)&&!pm04_accept(&s,&a)&&!pm04_edges0(&s,7,10)&&!pm04_level4(&s,7,false)&&!memcmp(&s,&old,sizeof s));
 ready(&s,false);assert(pm04_level4(&s,7,true)&&pm04_reserve(&s,&a)&&pm04_accept(&s,&a));wr(&s,0x20,32);assert(!s.irr&&s.level4&&!pm04_peek(&s,&n,&v));assert(pm04_level4(&s,7,true)&&!s.irr); /* EOI not a rising edge */
 ready(&s,true);assert(pm04_edges0(&s,7,100)&&pm04_reserve(&s,&a)&&pm04_edges0(&s,7,102)&&pm04_accept(&s,&a)&&s.irr==1&&!s.isr);
 ready(&s,false);wr(&s,0x21,255);assert(pm04_edges0(&s,7,2)&&pm04_level4(&s,7,true)&&s.irr==17&&!pm04_peek(&s,&n,&v));wr(&s,0x21,0xea);assert(pm04_peek(&s,&n,&v)&&n==0);
 for(unsigned m=0;m<256;m++){ready(&s,false);uint8_t x=m;old=s;bool ok=(m&0xea)==0xea;assert(pm04_access(&s,0x21,true,&x,true)==ok);if(!ok)assert(!memcmp(&s,&old,sizeof s));}
 for(unsigned m=0;m<256;m++){ready(&s,false);uint8_t x=m;old=s;bool ok=m==17||m==10||m==11||m==32||m==96||m==100;assert(pm04_access(&s,0x20,true,&x,true)==ok);if(!ok)assert(!memcmp(&s,&old,sizeof s));}
 ready(&s,false);v=0xea;old=s;assert(!pm04_access(&s,0x21,true,&v,false)&&!memcmp(&s,&old,sizeof s));
 assert(pm04_level4(&s,7,true));wr(&s,0x20,17);assert(!s.irr&&s.level4);assert(pm04_edges0(&s,7,5)&&pm04_level4(&s,7,false)&&pm04_level4(&s,7,true)&&!s.irr);wr(&s,0x21,56);wr(&s,0x21,4);wr(&s,0x21,1);wr(&s,0x21,0xea);assert(!s.irr);assert(pm04_level4(&s,7,true)&&!s.irr);
 ready(&s,false);s.seen4=UINT64_MAX;old=s;assert(!pm04_level4(&s,7,true)&&!memcmp(&s,&old,sizeof s));assert(!pm04_edges0(&s,8,1));assert(pm04_edges0(&s,7,2));old=s;assert(!pm04_edges0(&s,7,1)&&!memcmp(&s,&old,sizeof s));s.serial=UINT64_MAX;old=s;assert(!pm04_reserve(&s,&a)&&!memcmp(&s,&old,sizeof s));
 for(unsigned mode=0;mode<2;mode++){assert(pm04_init(&s,7,0,false));wr(&s,0x21,mode?251:255);assert(!pm04_peek(&s,&n,&v));}
 puts("PIC master IRQ0/4 pure host PASS; not wired/VM");
}
