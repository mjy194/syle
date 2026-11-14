#include "../firmware_active_cpu_tsc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void check(struct fcp_host*h,uint32_t leaf,uint32_t sub){struct fcp_result a,b;assert(fcad_query(h,leaf,sub,&a)&&fcats_query(h,true,leaf,sub,&b));
 if(leaf==1||(leaf>1&&leaf<0x80000000u)||leaf>0x80000008u){assert(!(a.d&16));a.d|=16;}
 assert(!memcmp(&a,&b,sizeof a));}
int main(void){struct fcp_host h={~0u,~0u,1,0x80000008u,48,48};
 for(unsigned i=0;i<1024;i++){check(&h,i,i);check(&h,0x80000000u+i,i);check(&h,UINT32_MAX-i,i);}
 uint32_t boundaries[]={0x7fffffff,0x80000000,0x80000001,0x80000007,0x80000008,0x80000009,0xffffffff};
 for(unsigned i=0;i<sizeof boundaries/sizeof *boundaries;i++)check(&h,boundaries[i],0);
 struct fcp_result out,old;memset(&out,0x55,sizeof out);old=out;h.basic_edx&=~16u;
 assert(!fcats_supported(&h)&&fcad_supported(&h));assert(!fcats_query(&h,true,1,0,&out)&&!memcmp(&out,&old,sizeof out));
 assert(!fcats_supported(0)&&!fcats_query(0,true,1,0,&out));h.basic_edx|=16;h.basic_edx&=~(1u<<9);assert(!fcats_supported(&h));
 assert(!fcats_query(&h,true,1,0,0));h.basic_edx=~0u;assert(fcats_supported(&h));assert(!fcats_query(&h,false,1,0,&out));puts("firmware_active_cpu_tsc: PASS");}
