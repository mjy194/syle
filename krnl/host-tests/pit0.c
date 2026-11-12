#include "pit0.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put(struct pit0*s,uint64_t t,unsigned p,uint8_t v){assert(pit0_access(s,t,p,true,&v));}
static struct pit0 timer(unsigned n){struct pit0 s;pit0_init(&s);put(&s,0,0x43,0x34);put(&s,0,0x40,n);put(&s,0,0x40,n>>8);return s;}
int main(void){
 for(unsigned n=2;n<200;n++){
  struct pit0 a=timer(n),b=a;
  for(unsigned t=0;t<2000;t++){bool out;uint16_t count;assert(pit0_advance(&a,t));assert(pit0_output(&a,&out));assert(pit0_count(&a,&count));
   assert(a.edges==(t<1?0:(t-1)/n));assert(out==(t<1||((t-1)%n)!=n-1));assert(count==(t<1?n:n-(t-1)%n));}
  assert(pit0_advance(&b,1999));assert(a.edges==b.edges&&a.start==b.start);
 }
 struct pit0 s=timer(4);assert(pit0_advance(&s,3));put(&s,3,0x40,2);put(&s,3,0x40,0);assert(s.reload==4&&s.pending==2);assert(pit0_advance(&s,5));assert(s.reload==2&&s.edges==1);assert(pit0_advance(&s,9)&&s.edges==3);
 s=timer(0);assert(s.reload==65536);put(&s,1,0x43,0);uint8_t lo=0,hi=0;assert(pit0_access(&s,4,0x40,false,&lo));assert(pit0_access(&s,8,0x40,false,&hi));assert(!lo&&!hi);
 struct pit0 old=s;assert(!pit0_advance(&s,1)&&!memcmp(&old,&s,sizeof s));uint8_t v=0x35;assert(!pit0_access(&s,9,0x43,true,&v)&&!memcmp(&old,&s,sizeof s));
 s=timer(4);assert(pit0_advance(&s,4));put(&s,4,0x43,0x34);assert(s.edges==1&&!s.running); /* low-to-high programming transition */
 s=timer(2);s.edges=UINT64_MAX;old=s;assert(!pit0_advance(&s,3)&&!memcmp(&old,&s,sizeof s));
 s=timer(4);put(&s,0,0x40,6);put(&s,0,0x40,0);assert(s.reload==6&&!s.pending);assert(pit0_advance(&s,7)&&s.edges==1);
 s=timer(4);v=0;assert(pit0_access(&s,1,0x40,false,&v));old=s;v=0;assert(!pit0_access(&s,1,0x43,true,&v)&&!memcmp(&old,&s,sizeof s));
 puts("PIT0 isolated cloud candidate: count/OUT/edges differential, deferred reload, latch, fail atomic PASS; NOT runtime integrated");
}
