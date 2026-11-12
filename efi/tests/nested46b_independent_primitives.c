/* Independently execute production cache-maintenance logic with only privileged
 * reads/instructions substituted. Real cache_page.c scans synthetic tables. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "cache_maintenance.h"
#include "paging.h"
#include "vmx_memory.h"
#define ROOT UINT64_C(0x1000000)
#define TARGET UINT64_C(0xf000)
#define NX (UINT64_C(1)<<63)
static _Alignas(4096) uint64_t tables[4][512];
static struct cache_cpu_snapshot fake;
static uint64_t fake_flags, root, last_inv;
static unsigned inv_calls,cache_calls,mtrr_calls,reads,cases;
static bool wb;
static void *forbidden;
void *paging_phys_to_virt(uint64_t p){++reads;if(p==TARGET)return forbidden;
 if(p>=ROOT&&p<ROOT+sizeof tables&&!(p&4095))return tables[(p-ROOT)/4096];return NULL;}
uint64_t paging_root(void){return root;}
int vmx_memory_region_wb(uint64_t p){assert(p==TARGET);++mtrr_calls;return wb;}
#include "cache_maintenance_injected.c"
static void reset(void){memset(tables,0,sizeof tables);for(unsigned n=0;n<3;n++)tables[n][0]=(ROOT+(n+1)*4096)|3;
 tables[3][15]=TARGET|NX|3;fake=(struct cache_cpu_snapshot){.cr0=0x80010033,.cr3=ROOT,.cr4=0x2000,.pat=UINT64_C(0x0007040600070406),.apic=0};
 root=ROOT;fake_flags=2;wb=true;reads=inv_calls=cache_calls=mtrr_calls=0;last_inv=0;}
static void test_primitive_guards(void){
 for(unsigned cpu=0;cpu<2;cpu++)for(unsigned fault=0;fault<13;fault++){
  reset();fake.apic=cpu;struct cache_cpu_snapshot s;struct cache_page_lease l;assert(cache_maintenance_precheck(TARGET,&s));assert(mtrr_calls==1);
  switch(fault){case 0:break;case 1:fake.apic^=1;break;case 2:fake.cr3+=4096;break;case 3:root+=4096;break;case 4:fake.pat^=256;break;
   case 5:fake.cr0|=UINT64_C(1)<<29;break;case 6:fake.cr0|=UINT64_C(1)<<30;break;case 7:fake.cr4|=1u<<7;break;
   case 8:fake.cr4|=1u<<12;break;case 9:fake.cr4|=1u<<17;break;case 10:fake_flags|=0x200;break;case 11:s.mtrr_wb=false;break;case 12:fake.cr3+=4096;root+=4096;break;}
  bool ok=fault==0;assert(cache_maintenance_tlb(&s)==ok);assert(cache_maintenance_cache(&s)==ok);assert(cache_maintenance_lease(&s,&l)==ok);
  tables[3][15]|=0x18;assert(cache_maintenance_lease_current(&s,0,&l)==ok);
  assert(inv_calls==ok&&cache_calls==ok);if(ok)assert(last_inv==TARGET);assert(mtrr_calls==1);cases++;
 }
 puts("PASS actual stable/INVLPG/WBINVD/lease functions: both CPU identities, CD/NW/PGE/LA57/PCIDE/IF/PAT/CR3 and initial MTRR proof");
}
static void test_host_alias(void){
 for(unsigned fault=0;fault<17;fault++){
  reset();struct cache_cpu_snapshot s;struct cache_page_lease l;assert(cache_maintenance_precheck(TARGET,&s));tables[3][15]|=0x18;
  switch(fault){case 0:break;case 1:tables[3][15]=0;break;case 2:tables[3][16]=tables[3][15];break;
   case 3:tables[3][15]&=~2ull;break;case 4:tables[3][15]&=~NX;break;case 5:tables[3][15]&=~0x18ull;break;
   case 6:tables[3][15]|=0x80;break;case 7:tables[3][15]^=4096;break;case 8:tables[2][0]=TARGET|3;break;
   case 9:tables[2][0]=0x83;break;case 10:tables[1][0]=0x83;break;case 11:tables[1][0]=ROOT|3;break;
   case 12:tables[0][0]|=8;break;case 13:tables[1][0]|=0x10;break;case 14:tables[2][0]&=~2ull;break;
   case 15:tables[3][15]|=1ull<<52;break;case 16:tables[3][15]^=0x60;break;}
  assert(cache_maintenance_lease_current(&s,0,&l)==(fault==0||fault==16));assert(mtrr_calls==1);cases++;
 }
 reset();struct cache_cpu_snapshot s;assert(cache_maintenance_precheck(TARGET,&s));tables[3][15]&=~1ull;
 assert(cache_maintenance_tlb(&s)&&cache_maintenance_cache(&s));tables[3][15]|=0x19;
 assert(cache_maintenance_tlb(&s)&&cache_maintenance_cache(&s));assert(mtrr_calls==1);cases++;
 puts("PASS actual unique host-alias audits and absent interval with protected data, huge/cycle/table alias denial, ignored hardware A/D");
}
static void test_snapshot(void){
 reset();struct cache_cpu_snapshot s,t;assert(cache_maintenance_precheck(TARGET,&s));fake.apic=1;assert(cache_maintenance_precheck(TARGET,&t));
 for(unsigned f=0;f<7;f++){struct cache_cpu_snapshot x=t;switch(f){case 0:break;case 1:x.apic=0;break;case 2:x.cr3+=4096;break;
 case 3:x.pat^=256;break;case 4:x.target+=4096;break;case 5:x.mtrr_wb=false;break;case 6:x.cr0^=1;break;}
 assert(cache_maintenance_same(&s,&x)==(f==0||f==6));cases++;}
 reset();struct cache_cpu_snapshot out;memset(&out,0xa5,sizeof out);struct cache_cpu_snapshot before=out;wb=false;
 assert(!cache_maintenance_precheck(TARGET,&out));assert(!memcmp(&out,&before,sizeof out));cases++;
 puts("PASS snapshot policy agreement and failed precheck preserves output");
}
static uint64_t fields[4];static unsigned read_fail;
static bool get(uint64_t f,uint64_t *v){unsigned n=f==0x6800?0:f==0x6000?1:f==0x6004?2:3;assert(f==0x6800||f==0x6000||f==0x6004||f==0x4002);if(read_fail==n+1)return false;*v=fields[n];return true;}
#include "entry_guard.h"
static void test_entry(void){
 for(unsigned vmcs=0;vmcs<2;vmcs++)for(unsigned resume=0;resume<2;resume++)for(unsigned f=0;f<12;f++){
  fields[0]=0x80010033;fields[1]=0x60000000;fields[2]=0x80010033;fields[3]=0;read_fail=0;
  switch(f){case 0:break;case 1:fields[0]|=1u<<29;break;case 2:fields[0]|=1u<<30;break;
   case 3:fields[1]&=~(1u<<29);break;case 4:fields[1]&=~(1u<<30);break;
   case 5:fields[2]|=1u<<29;break;case 6:fields[2]|=1u<<30;break;case 7:fields[3]|=1u<<28;break;
   default:read_fail=f-7;break;}
  assert(actual_entry_guard()==(f==0));cases++;
 }
 puts("PASS exact physical entry-loop guard: CD/NW guest/shadow/mask, all-MSR intercept, VMREAD failures (same unconditional predicate on VMCS01/02 and internal resume)");
}
int main(void){forbidden=mmap(NULL,4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(forbidden!=MAP_FAILED);
 test_primitive_guards();test_host_alias();test_snapshot();test_entry();assert(!munmap(forbidden,4096));printf("INDEPENDENT_PRIMITIVES_OK cases=%u hardware_claim=false\n",cases);return 0;}
