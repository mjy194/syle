#include "../firmware_active_cpu_de.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void check(struct fcp_host*h,uint32_t leaf,uint32_t sub){struct fcp_result a,b;assert(fca_query(h,leaf,sub,&a)&&fcad_query(h,leaf,sub,&b));
 if(leaf==1||(leaf>1&&leaf<0x80000000u)||leaf>0x80000008u){assert(!(a.d&4));a.d|=4;}
 assert(!memcmp(&a,&b,sizeof a));}
int main(void){struct fcp_host h={~0u,~0u,1,0x80000008u,48,48};
 for(unsigned i=0;i<1024;i++){check(&h,i,i);check(&h,0x80000000u+i,i);check(&h,UINT32_MAX-i,i);}
 uint32_t boundaries[]={0x7fffffff,0x80000000,0x80000001,0x80000007,0x80000008,0x80000009,0xffffffff};
 for(unsigned i=0;i<sizeof boundaries/sizeof *boundaries;i++)check(&h,boundaries[i],0);
 struct fcp_result out,old;memset(&out,0x55,sizeof out);old=out;h.basic_edx&=~4u;
 assert(!fcad_supported(&h)&&fca_supported(&h));assert(!fcad_query(&h,1,0,&out)&&!memcmp(&out,&old,sizeof out));
 assert(!fcad_supported(0)&&!fcad_query(0,1,0,&out));h.basic_edx|=4;h.basic_edx&=~(1u<<9);assert(!fcad_supported(&h));
 assert(!fcad_query(&h,1,0,0));puts("firmware_active_cpu_de: PASS");}
