#include "pic0_glue.h"
#include <assert.h>
#include <stdio.h>
struct mock {unsigned calls,fail;struct pgg*g;bool reenter;};
static bool wr(void*p,uint32_t f,uint64_t v){struct mock*m=p;(void)f;(void)v;if(m->reenter)assert(!pgg_after(m->g,PGG_EXIT,10,0,p,wr));return ++m->calls!=m->fail;}
static void cmd(struct pic0*p,unsigned port,uint8_t x){assert(pic0_access(p,port,true,&x));}
static struct pic0 pic;static struct pgg glue;static struct mock mock;
static struct lei_snapshot snapshot;
static void setup(void){assert(pic0_init(&pic,7,0));cmd(&pic,0x20,0x11);cmd(&pic,0x21,0x30);cmd(&pic,0x21,4);cmd(&pic,0x21,1);cmd(&pic,0x21,0xfe);assert(pic0_edges(&pic,7,1));assert(pgg_init(&glue,&pic));mock=(struct mock){.g=&glue};snapshot=(struct lei_snapshot){.rflags=512};}
int main(void){setup();mock.reenter=true;assert(pgg_before(&glue,&snapshot,&mock,wr)==PGG_READY&&pic.reserved);assert(pic0_edges(&pic,7,2));assert(pgg_after(&glue,PGG_EXIT,10,0,&mock,wr)&&!pic.reserved&&pic.irr&&pic.isr);cmd(&pic,0x20,0x20);assert(pgg_before(&glue,&snapshot,&mock,wr)==PGG_READY);assert(pgg_after(&glue,PGG_NO_ENTRY,0,0,&mock,wr)&&pic.irr&&!pic.reserved&&!pic.isr);
 for(unsigned f=1;f<=6;f++){setup();mock.fail=f;enum pgg_before_result b=pgg_before(&glue,&snapshot,&mock,wr);if(f<=3)assert(b==PGG_ERROR);else{assert(b==PGG_READY);assert(!pgg_after(&glue,PGG_EXIT,10,0,&mock,wr));}assert(glue.failed&&pic.reserved&&pic.irr&&!pic.isr);unsigned c=mock.calls;assert(!pgg_after(&glue,PGG_EXIT,10,0,&mock,wr)&&mock.calls==c);}
 for(unsigned f=4;f<=6;f++){setup();mock.fail=f;assert(pgg_before(&glue,&snapshot,&mock,wr)==PGG_READY);assert(!pgg_after(&glue,PGG_NO_ENTRY,0,0,&mock,wr)&&pic.reserved&&pic.irr&&glue.failed);}
 setup();snapshot.pf_pending=true;assert(pgg_before(&glue,&snapshot,&mock,wr)==PGG_READY&&!pic.reserved&&!mock.calls);
 setup();snapshot.rflags=0;assert(pgg_before(&glue,&snapshot,&mock,wr)==PGG_DEFER_WINDOW&&!pic.reserved&&!mock.calls);
 setup();snapshot.activity=1;assert(pgg_before(&glue,&snapshot,&mock,wr)==PGG_READY&&pic.reserved);assert(pgg_after(&glue,PGG_NO_ENTRY,0,0,&mock,wr)&&pic.irr);
 setup();snapshot.activity=1;snapshot.blocking=1;assert(pgg_before(&glue,&snapshot,&mock,wr)==PGG_ERROR&&glue.failed);
 setup();snapshot.activity=2;assert(pgg_before(&glue,&snapshot,&mock,wr)==PGG_ERROR&&glue.failed);
 const unsigned bad[]={0,7,52,0x80000021};for(unsigned i=0;i<4;i++){setup();assert(pgg_before(&glue,&snapshot,&mock,wr)==PGG_READY);if(bad[i]==0x80000021){assert(pgg_after(&glue,PGG_EXIT,bad[i],0,&mock,wr)&&!pic.reserved&&pic.irr);}else assert(!pgg_after(&glue,PGG_EXIT,bad[i],0,&mock,wr)&&pic.reserved&&glue.failed);}
 setup();assert(pgg_before(&glue,&snapshot,&mock,wr)==PGG_READY);assert(!pgg_after(&glue,PGG_UNKNOWN,0,0,&mock,wr)&&pic.reserved);
 setup();assert(pgg_before(&glue,&snapshot,&mock,wr)==PGG_READY);assert(!pgg_after(&glue,PGG_EXIT,10,0x80000030,&mock,wr)&&pic.reserved);
 puts("PIC0+LEI host glue: reservation/receipt/clear/account, noentry, postreserve edges, PF/defer, writefail and quarantine PASS; halted gate only; no VM acceptance");return 0;}
