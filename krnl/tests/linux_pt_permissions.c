#include "../linux_pt_permissions.h"
#include <assert.h>
#include <stdio.h>
static uint64_t m[4096];
static bool rd(void*x,uint64_t a,uint64_t*v){(void)x;assert(a<sizeof m&&!(a&7));*v=m[a/8];return true;}
#define CHECK(a,ef,wp) lpp_range(0,0,4096,a,0x80000033ULL|(wp),0x20,ef,32768,0,rd)
int main(void){m[0]=4099;m[512]=8195;m[1024]=12291;m[1536]=3;
 assert(CHECK(0,0xd01,0));assert(CHECK(1,0xd01,0x10000));assert(CHECK(2,0xd01,0));
 m[0]&=~2ULL;assert(!CHECK(1,0xd01,0x10000));assert(CHECK(1,0xd01,0));m[0]|=2;
 m[512]|=1ULL<<63;assert(CHECK(0,0xd01,0));assert(!CHECK(2,0xd01,0));assert(!CHECK(0,0x501,0));m[512]&=~(1ULL<<63);
 assert(!lpp_range(0,0,4097,0,0x80000033,0x20,0xd01,32768,0,rd));
 assert(!lpp_range(0,0,65537,0,0x80000033,0x20,0xd01,32768,0,rd));
 assert(!lpp_range(0,UINT64_MAX,2,0,0x80000033,0x20,0xd01,32768,0,rd));
 assert(!lpp_range(0,0,1,0,0x80000033,0x1020,0xd01,32768,0,rd));
 m[512]|=128;assert(!CHECK(0,0xd01,0));m[512]&=~128ULL;
 m[1536]|=1ULL<<32;assert(!CHECK(0,0xd01,0));
 puts("linux_pt_permissions: PASS");}
