#include "../linux_host_clock_hv.h"
#include <assert.h>
#include <stdio.h>
static unsigned defect,reads;static uint64_t counter;
static bool cpu(void*p,uint32_t leaf,uint32_t*r){(void)p;for(unsigned i=0;i<4;i++)r[i]=0;
 if(leaf==1){r[1]=3u<<24;r[2]=1u<<31;if(defect==1)r[2]=0;if(defect==7)r[1]=4u<<24;}
 if(leaf==0x40000000)r[0]=defect==2?0x40000002:0x40000003;
 if(leaf==0x40000001)r[0]=defect==3?0:0x31237648;
 if(leaf==0x40000003)r[0]=defect==4?0:2;
 return defect!=8;
}
static bool reference(void*p,uint64_t*out){(void)p;++reads;if(defect==5)return false;*out=defect==6?100-reads:counter++;return true;}
int main(void){
 struct lhch_context c={0};struct lhcr_source source;struct lhch_ops ops={0,cpu,reference};
 for(defect=1;defect<=6;defect++){reads=0;assert(!lhch_admit(&c,&ops,1,&source));if(defect<=4)assert(!reads);}
 defect=0;counter=100;assert(lhch_admit(&c,&ops,1,&source)&&source.properties.hz==10000000);
 struct lhc_sample v;assert(source.sample(source.ctx,&v)&&v.cpu==3);defect=7;assert(!source.sample(source.ctx,&v));defect=8;assert(!source.sample(source.ctx,&v));
 puts("Hyper-V advertised reference-counter admission/fault/cpu rejection PASS");return 0;
}
