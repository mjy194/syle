#include "bounded_span.h"
#include <assert.h>
#include <stdio.h>
static void check(uint64_t ram,uint64_t start,uint64_t n){
 struct vbs s;assert(vbs_init(&s,ram,start,n));uint64_t p,seen=0;size_t o,k;
 while(vbs_next(&s,&p,&o,&k)){
  assert(k&&o<4096&&k<=4096-o);assert(p*4096+o==start+seen);
  assert(k<=n-seen);seen+=k;
 }assert(seen==n);assert(s.remaining==0);assert(s.cursor==start+n);
}
int main(void){
 struct vbs s={7,9};assert(!vbs_init(&s,8192,8193,0));assert(s.cursor==7&&s.remaining==9);
 assert(!vbs_init(&s,8192,8191,2));assert(!vbs_init(&s,8192,1,UINT64_MAX));
 check(8192,8192,0);check(8192,8191,1);check(8192,4095,4097);
 check(UINT64_MAX,UINT64_MAX-1,1);check(UINT64_MAX,UINT64_MAX,0);
 for(uint64_t i=0;i<8192;i++)for(uint64_t n=0;n<17&&n<=8192-i;n++)check(8192,i,n);
 check(128*1024*1024,0,128*1024*1024);
 assert(vbs_init(&s,8192,0,1));assert(!vbs_next(&s,0,0,0));assert(s.remaining==1);
 uint64_t page=123;size_t off=456,len=789;
 s=(struct vbs){UINT64_MAX,1};assert(!vbs_next(&s,&page,&off,&len));
 assert(s.cursor==UINT64_MAX&&s.remaining==1&&page==123&&off==456&&len==789);
 s=(struct vbs){0,1};assert(!vbs_next(&s,&page,0,&len));
 assert(s.cursor==0&&s.remaining==1&&page==123&&len==789);
 puts("bounded span geometry PASS; no capability or mapping admission");return 0;
}
