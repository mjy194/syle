#include "linux_pt_diag.h"
void lpt_walk(uint64_t cr3,uint64_t va,uint64_t ram,void*ctx,bool(*read)(void*,uint64_t,uint64_t*),struct lpt_trace*t){
 if(!t)return;
 *t=(struct lpt_trace){0};t->status=3;
 if(!read||ram<4096||((va>>47)!=0&&(va>>47)!=0x1ffff)||(cr3&~0x00000000fffff000ULL))return;
 uint64_t table=cr3;
 for(unsigned level=0;level<4;level++){
  unsigned shift=39-9*level;uint64_t addr=table+((va>>shift)&511)*8;
  if(table>=ram||ram-table<4096||addr>ram-8){t->status=4;return;}
  t->address[level]=addr;if(!read(ctx,addr,&t->entry[level])){t->status=4;return;}t->count++;
  uint64_t e=t->entry[level];if(!(e&1)){t->status=2;return;}
  if(e&0x7fffffff00000000ULL)return;
  if((e&128)&&(level==0||level==1))return; /* guest hides 1GiB pages */
  if(level==3||((e&128)&&(level==1||level==2))){
   uint64_t mask=(1ULL<<shift)-1;
   if(level!=3&&(e&(mask&~0x1fffULL)))return;
   t->gpa=(e&0x00000000fffff000ULL&~mask)|(va&mask);
   t->status=t->gpa<ram?1:4;return;
  }
  table=e&0x00000000fffff000ULL;
 }
}
