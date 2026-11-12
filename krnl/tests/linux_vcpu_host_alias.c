/* Reuse model setup only, not its existing test main. This dedicated test runs
 * new host-validation/alias cases introduced by the review fixes. */
#define main prior_model_main_not_executed
#include "linux_vcpu_host.c"
#undef main
static unsigned newcase,alloc_calls,map_calls,wb_calls,release_calls;
static uint64_t allocated[2];
static bool adjusted_capture(void*x,struct lvh_host*h){if(!capture(x,h))return false;if(newcase==1)h->cr4|=1u<<17;if(newcase>=2&&newcase<=13)h->cr3|=1u<<(newcase-2);return true;}
static uint64_t alias_alloc(void*x){(void)x;unsigned i=alloc_calls++;assert(i<2);uint64_t p=0x100000+i*4096;
 if(newcase==20)p=ram.pages[0];
 if(newcase==21)p=ram.pages[LGM_PAGE_COUNT-1];
 if(newcase==22)p=ram.tables[LGM_TABLE_COUNT-1];
 if(newcase==23&&i==1)p=allocated[0];
 if(newcase==24)p=ram.pages[3]+1;
 allocated[i]=p;return p;}
static bool counted_wb(void*x,uint64_t p){(void)x;(void)p;wb_calls++;return true;}
static void*counted_map(void*x,uint64_t p){(void)x;map_calls++;return m.page[p==0x101000];}
static bool counted_free(void*x,uint64_t p){(void)x;(void)p;release_calls++;return true;}
static void setup(void){memset(&m,0,sizeof m);memset(&c,0,sizeof c);alloc_calls=map_calls=wb_calls=release_calls=0;
 ops.capture=adjusted_capture;ops.memory=(struct sg_memory_ops){0,alias_alloc,counted_free,counted_map,counted_wb};
 ram.prepared=true;ram.plan.ram_size=LGM_RAM_SIZE;ram.plan.kernel_addr=0x1000000;ram.plan.kernel_size=0x100000;ram.plan.kernel_envelope=0xa00000;ram.plan.entry_addr=0x1000200;ram.plan.params_addr=0x90000;
 ram.entry=(struct lb_entry_state){0x1000200,0x90000,0x80000,0x80000033,0x1000,0x620,0x500,0x4000,47,0x10,0x18};
 for(unsigned i=0;i<LGM_PAGE_COUNT;i++)ram.pages[i]=0x10000000ull+4096ull*i;
 for(unsigned i=0;i<LGM_TABLE_COUNT;i++)ram.tables[i]=0x30000000ull+4096ull*i;
 ram.eptp=ram.tables[0]|0x1e;
}
int main(void){
 /* PCIDE and CR3 low reserved bits0..2,5..11 rejected before allocation.
  * PWT/PCD bits3/4 are architectural cache controls, not rejected here. */
 unsigned bad[]={1,2,3,4,7,8,9,10,11,12,13};
 for(unsigned i=0;i<sizeof bad/sizeof*bad;i++){newcase=bad[i];setup();assert(!lvh_open(&c,&ops,&ram,0x80000,0x400000));assert(!alloc_calls&&!m.active&&!c.quarantined);}
 for(newcase=20;newcase<=24;newcase++){setup();memset(m.page,0xa5,sizeof m.page);assert(!lvh_open(&c,&ops,&ram,0x80000,0x400000));assert(c.quarantined&&!c.active&&!c.ready&&!release_calls);assert(!lvh_close(&c));assert(!release_calls);assert(!lvh_open(&c,&ops,&ram,0x80000,0x400000));
  if(newcase==23){assert(alloc_calls==2&&map_calls==1&&wb_calls==1);assert(m.page[0][0]==1);}
  else{assert(alloc_calls==1&&!map_calls&&!wb_calls);for(unsigned i=0;i<sizeof m.page;i++)assert(((unsigned char*)m.page)[i]==0xa5);}
 }
 puts("PASS new host safeguards: PCIDE/reserved CR3 rejection before allocation; RAM/EPT/duplicate/misaligned aliases quarantined before map/zero/free; no hardware");return 0;
}
