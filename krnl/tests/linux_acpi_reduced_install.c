#include "../linux_acpi_reduced_install.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint8_t ram[0x100000];struct fake{unsigned calls,fail,writes;bool corrupt;};
static void put(uint8_t*p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=v>>(8*i);}
static bool readram(void*p,uint64_t a,void*b,size_t n){struct fake*f=p;assert(a<sizeof ram&&n<=sizeof ram-a);if(++f->calls==f->fail)return false;memcpy(b,ram+a,n);if(f->corrupt&&a==LAT_GPA)((uint8_t*)b)[256]^=1;return true;}
static bool writeram(void*p,uint64_t a,const void*b,size_t n){struct fake*f=p;assert((a==LAT_GPA&&n==LAR_SIZE)||(a==0x90070&&n==8));f->writes++;memcpy(ram+a,b,n);return ++f->calls!=f->fail;}
static void setup(void){memset(ram,0,sizeof ram);uint8_t*b=ram+0x90000;put(b+0x202,0x53726448,4);put(b+0x206,0x20f,2);b[0x1e8]=1;put(b+0x2d0,0x9f000,8);put(b+0x2d8,0x61000,8);put(b+0x2e0,2,4);}
int main(void){for(unsigned fail=0;fail<=6;fail++){setup();struct fake f={.fail=fail};struct lai_ops o={&f,readram,writeram};enum lai_state s=LAI_FRESH;assert(lar_install(&s,&o,0x8000000,0x90000)==(fail==0));assert(s==(fail==0?LAI_INSTALLED:fail==1?LAI_FRESH:LAI_DIRTY));if(s!=LAI_FRESH){unsigned old=f.calls;assert(!lar_install(&s,&o,0x8000000,0x90000)&&f.calls==old);}}
 setup();struct fake f={0};struct lai_ops o={&f,readram,writeram};enum lai_state s=LAI_FRESH;assert(lar_install(&s,&o,0x8000000,0x90000));o.write=0;assert(lar_verify_owned(&o,0x8000000,0x90000));unsigned w=f.writes;
 for(unsigned i=0;i<LAR_SIZE;i++){ram[LAT_GPA+i]^=1;assert(!lar_verify_owned(&o,0x8000000,0x90000));ram[LAT_GPA+i]^=1;}assert(f.writes==w);
 /* Even recomputed checksum cannot authorize a different FADT policy. */
 ram[LAT_GPA+256+112]^=1;ram[LAT_GPA+256+9]-=1;assert(!lar_verify_owned(&o,0x8000000,0x90000));ram[LAT_GPA+256+112]^=1;ram[LAT_GPA+256+9]+=1;
 ram[0x90070]^=1;assert(!lar_verify_owned(&o,0x8000000,0x90000));ram[0x90070]^=1;ram[0x902e0]=1;assert(!lar_verify_owned(&o,0x8000000,0x90000));
 setup();f=(struct fake){.corrupt=true};o=(struct lai_ops){&f,readram,writeram};s=LAI_FRESH;assert(!lar_install(&s,&o,0x8000000,0x90000)&&s==LAI_DIRTY&&f.writes==1&&!ram[0x90070]);
 setup();ram[0x90070]=1;f=(struct fake){0};s=LAI_FRESH;assert(!lar_install(&s,&o,0x8000000,0x90000)&&s==LAI_FRESH&&!f.writes);
 puts("reduced ACPI install/exact owned-RAM verification host tests PASS");}
