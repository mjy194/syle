#include "pic_master_04_glue.h"
#include <assert.h>
#include <stdio.h>
struct mock{unsigned writes,fail;};
static bool wr(void*x,uint32_t f,uint64_t v){(void)f;(void)v;struct mock*m=x;return ++m->writes!=m->fail;}
static void out(struct pic_master_04*p,unsigned port,unsigned b){uint8_t v=b;assert(pm04_access(p,port,true,&v,true));}
static void setup(struct pic_master_04*p,struct pmg*g){assert(pm04_init(p,9,0,false));out(p,32,17);out(p,33,48);out(p,33,4);out(p,33,1);out(p,33,0xea);assert(pm04_level4(p,9,true));assert(pmg_init(g,p));}
int main(void){struct pic_master_04 p;struct pmg g;struct mock m={0};struct lei_snapshot s={.rflags=512};
 setup(&p,&g);assert(pmg_before(&g,&p,&s,&m,wr)==PMG_READY&&p.reserved.source==4);assert(pm04_edges0(&p,9,1));assert(pmg_after(&g,&p,PMG_EXIT,30,0,&m,wr)&&p.isr==16&&p.irr==1);assert(pmg_before(&g,&p,&s,&m,wr)==PMG_READY&&p.reserved.source==0);assert(pmg_after(&g,&p,PMG_NO_ENTRY,0,0,&m,wr)&&p.irr==1&&p.isr==16&&!p.reserved.serial);
 for(unsigned f=1;f<=6;f++){setup(&p,&g);m=(struct mock){.fail=f};enum pmg_before_result r=pmg_before(&g,&p,&s,&m,wr);if(f<=3)assert(r==PMG_ERROR&&g.failed&&p.reserved.serial&&!p.isr);else{assert(r==PMG_READY);assert(!pmg_after(&g,&p,PMG_EXIT,30,0,&m,wr)&&g.failed&&p.reserved.serial&&!p.isr);}assert(!pmg_after(&g,&p,PMG_EXIT,30,0,&m,wr));}
 for(unsigned f=0;f<4;f++){setup(&p,&g);m=(struct mock){0};assert(pmg_before(&g,&p,&s,&m,wr)==PMG_READY);assert(!pmg_after(&g,&p,f==3?PMG_UNKNOWN:PMG_EXIT,f==0?52:f==1?7:30,f==2?0x80000020:0,&m,wr)&&g.failed&&p.reserved.serial&&p.quarantined);}
 setup(&p,&g);m=(struct mock){0};s.rflags=0;assert(pmg_before(&g,&p,&s,&m,wr)==PMG_DEFER_WINDOW&&!m.writes&&!p.reserved.serial);s.rflags=512;
 setup(&p,&g);m=(struct mock){0};s.pf_pending=true;s.entry_info=0x80000b0e;assert(pmg_before(&g,&p,&s,&m,wr)==PMG_READY&&!m.writes);s.pf_pending=false;assert(pmg_before(&g,&p,&s,&m,wr)==PMG_ERROR&&!m.writes);
 setup(&p,&g);s.entry_info=0;s.vectoring=1;assert(pmg_before(&g,&p,&s,&m,wr)==PMG_ERROR);s.vectoring=0;
 setup(&p,&g);m=(struct mock){0};assert(pmg_before(&g,&p,&s,&m,wr)==PMG_READY);assert(pm04_level4(&p,9,false)&&pm04_level4(&p,9,true));assert(pmg_after(&g,&p,PMG_EXIT,16,0,&m,wr)&&p.irr==16&&p.isr==16);
 puts("PIC04 receipt bridge host PASS; no VM");
}
