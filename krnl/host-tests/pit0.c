#include "pit0.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put(struct pit0*s,uint64_t t,unsigned p,uint8_t v){assert(pit0_access(s,t,p,true,&v));}
static struct pit0 timer(unsigned n){struct pit0 s;pit0_init(&s);put(&s,0,0x43,0x34);put(&s,0,0x40,n);put(&s,0,0x40,n>>8);return s;}
static struct pit0 strobe(unsigned n){struct pit0 s;pit0_init(&s);put(&s,0,0x43,0x38);put(&s,0,0x40,n);put(&s,0,0x40,n>>8);return s;}
static void mode4_tests(void){
 for(unsigned n=1;n<100;n++){
  struct pit0 a=strobe(n),b=a;uint64_t edge;assert(a.mode4&&pit0_next_edge(&a,&edge)&&edge==n+2);
  for(unsigned t=0;t<70000;t++){
   bool out;uint16_t count;assert(pit0_advance(&a,t)&&pit0_output(&a,&out)&&pit0_count(&a,&count));
   assert(out==(t!=n+1));assert(a.edges==(t>=n+2));
   assert(count==(t<1?n:(uint16_t)(n-(uint16_t)(t-1))));
  }
  assert(!pit0_next_edge(&a,&edge));assert(pit0_advance(&b,69999));
  assert(a.edges==b.edges&&a.start==b.start&&a.strobe_done==b.strobe_done);
 }
 struct pit0 s=strobe(4);uint16_t count;bool out;uint64_t edge;
 assert(pit0_advance(&s,3));put(&s,3,0x40,2); /* first byte doesn't restart */
 assert(s.start==1&&s.reload==4&&pit0_count(&s,&count)&&count==2);
 put(&s,3,0x40,0);assert(s.start==4&&s.reload==2&&!s.pending);
 assert(pit0_count(&s,&count)&&count==2);assert(pit0_advance(&s,4)&&pit0_count(&s,&count)&&count==2);
 assert(pit0_advance(&s,6)&&pit0_output(&s,&out)&&!out&&!s.edges);
 /* Reload during low strobe keeps OUT low until next CLK, not immediate. */
 put(&s,6,0x40,3);put(&s,6,0x40,0);
 assert(pit0_output(&s,&out)&&!out&&pit0_next_edge(&s,&edge)&&edge==7);
 assert(pit0_advance(&s,7)&&pit0_output(&s,&out)&&out&&s.edges==1);
 assert(pit0_next_edge(&s,&edge)&&edge==11);
 assert(pit0_advance(&s,10)&&pit0_output(&s,&out)&&!out);
 assert(pit0_advance(&s,11)&&s.edges==2&&s.strobe_done);
 assert(pit0_advance(&s,100000)&&s.edges==2);
 /* Zero encodes 65536; latch snapshots wrap count, one edge only. */
 s=strobe(0);assert(s.reload==65536&&pit0_next_edge(&s,&edge)&&edge==65538);
 assert(pit0_advance(&s,65537)&&pit0_output(&s,&out)&&!out);put(&s,65537,0x43,0);
 uint8_t lo=1,hi=1;assert(pit0_access(&s,65538,0x40,false,&lo));assert(pit0_access(&s,65539,0x40,false,&hi));assert(!lo&&!hi&&s.edges==1);
 /* Mode reprogramming raises a low OUT and cancels the old strobe. */
 s=strobe(1);assert(pit0_advance(&s,2));put(&s,2,0x43,0x34);assert(s.edges==1&&!s.mode4&&!s.running);
 s=timer(2);assert(pit0_advance(&s,2));put(&s,2,0x43,0x38);assert(s.edges==1&&s.mode4&&!s.running);
 /* Invalid control, time, and unrepresentable edges are atomic rejections. */
 s=strobe(2);struct pit0 old=s;uint8_t v=0x39;
 assert(!pit0_access(&s,0,0x43,true,&v)&&!memcmp(&s,&old,sizeof s));
 assert(pit0_advance(&s,3));s.edges=UINT64_MAX;old=s;
 assert(!pit0_advance(&s,4)&&!memcmp(&s,&old,sizeof s));
 pit0_init(&s);put(&s,UINT64_MAX-2,0x43,0x38);put(&s,UINT64_MAX-2,0x40,1);old=s;v=0;
 assert(!pit0_access(&s,UINT64_MAX-2,0x40,true,&v)&&!memcmp(&s,&old,sizeof s));
}
int main(void){
 mode4_tests();
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
 puts("PIT0 mode2/mode4: count/OUT/edges differential, deferred reload, latch, fail atomic PASS; NOT runtime integrated");
}
