#include "../linux_pt_diag.h"
#include <assert.h>
#include <stdio.h>
static uint64_t m[4096];static unsigned reads;
static bool rd(void*x,uint64_t a,uint64_t*v){(void)x;assert(a<sizeof m&&!(a&7));reads++;*v=m[a/8];return true;}
int main(void){struct lpt_trace t;m[0]=4096|3;m[512]=8192|3;m[1024]=12288|3;m[1536]=3;
 lpt_walk(0,0,32768,0,rd,&t);assert(t.status==1&&t.gpa==0&&t.count==4);
 m[0]=0;lpt_walk(0,0,32768,0,rd,&t);assert(t.status==2&&t.count==1);
 m[0]=0x10000003;lpt_walk(0,0,32768,0,rd,&t);assert(t.status==4&&t.count==1);
 reads=0;lpt_walk(0,0x800000000000ULL,32768,0,rd,&t);assert(t.status==3&&!reads);
 lpt_walk(1,0,32768,0,rd,&t);assert(t.status==3&&!reads);
 m[0]=4096|3;m[512]=8192|3;m[1024]=0x83;lpt_walk(0,0x1234,32768,0,rd,&t);assert(t.status==1&&t.gpa==0x1234&&t.count==3);
 m[1024]=0x2083;lpt_walk(0,0,32768,0,rd,&t);assert(t.status==3);
 m[0]=0x83;lpt_walk(0,0,32768,0,rd,&t);assert(t.status==3);
 puts("linux_pt_diag: PASS");}
