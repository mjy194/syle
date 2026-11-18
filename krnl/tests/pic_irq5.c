#include "../pic_master_04.h"
#include <assert.h>
#include <stdio.h>
static void out(struct pic_master_04*s,unsigned port,unsigned value){uint8_t b=value;assert(pm04_access(s,port,true,&b,true));}
int main(void){struct pic_master_04 s;assert(pm04_init(&s,1,0,false));out(&s,0x20,0x11);out(&s,0x21,0x20);out(&s,0x21,4);out(&s,0x21,1);out(&s,0x21,0xca);
 assert(pm04_level5(&s,1,true));unsigned char source,vector;assert(pm04_peek(&s,&source,&vector)&&source==5&&vector==0x25);struct pm04_ticket t;assert(pm04_reserve(&s,&t)&&pm04_accept(&s,&t)&&s.isr==32);
 assert(pm04_level4(&s,1,true)&&pm04_peek(&s,&source,&vector)&&source==4);assert(pm04_reserve(&s,&t)&&pm04_accept(&s,&t)&&s.isr==48);assert(pm04_edges0(&s,1,1)&&pm04_peek(&s,&source,&vector)&&source==0);assert(pm04_reserve(&s,&t)&&pm04_accept(&s,&t)&&s.isr==49);
 out(&s,0x20,32);assert(s.isr==48);out(&s,0x20,32);assert(s.isr==32);out(&s,0x20,101);assert(!s.isr);
 assert(pm04_level5(&s,1,false)&&pm04_level5(&s,1,true));assert(pm04_reserve(&s,&t));assert(pm04_level5(&s,1,false)&&pm04_level5(&s,1,true));assert(pm04_accept(&s,&t)&&s.irr&32);out(&s,0x20,32);assert(pm04_peek(&s,&source,&vector)&&source==5);
 uint8_t b=0xc8;assert(!pm04_access(&s,0x21,true,&b,true));puts("PIC IRQ5: priority nesting, ISR/EOI, pending generation and mask bounds PASS");}
