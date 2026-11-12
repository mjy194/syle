#include "../linux_acpi_reduced.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint64_t get(const uint8_t*p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static bool sum(const uint8_t*p,unsigned n){unsigned v=0;for(unsigned i=0;i<n;i++)v+=p[i];return !(v&255);}
static bool validate(const uint8_t*b,size_t n){if(n!=LAR_SIZE||memcmp(b,"RSD PTR ",8)||!sum(b,20)||!sum(b,36)||get(b+24,8)!=LAT_GPA+64)return false;
 const uint8_t*x=b+64,*m=b+128,*f=b+256,*d=b+576;
 if(memcmp(x,"XSDT",4)||get(x+4,4)!=52||!sum(x,52)||get(x+36,8)!=LAT_GPA+256||get(x+44,8)!=LAT_GPA+128)return false;
 if(memcmp(m,"APIC",4)||get(m+4,4)!=74||!sum(m,74)||get(m+36,4)!=0xfee00000||m[54]||get(m+56,4)!=0xfec00000||get(m+72,2)!=13)return false;
 if(memcmp(f,"FACP",4)||get(f+4,4)!=276||f[8]!=6||f[131]!=5||!sum(f,276)||get(f+40,4)!=LAT_GPA+576||get(f+140,8)!=LAT_GPA+576||get(f+112,4)!=0x100030||get(f+109,2)!=0x28)return false;
 for(unsigned i=44;i<109;i++)if(f[i])return false;
 for(unsigned i=116;i<131;i++)if(f[i])return false;
 for(unsigned i=132;i<140;i++)if(f[i])return false;
 for(unsigned i=148;i<268;i++)if(f[i])return false;
 if(get(f+36,4)||memcmp(d,"DSDT",4)||get(d+4,4)!=136||d[8]!=2||!sum(d,136))return false;
 for(unsigned i=36;i+3<136;i++)if(d[i]=='_'&&d[i+1]=='S'&&d[i+2]>='1'&&d[i+2]<='5'&&d[i+3]=='_')return false;
 return true;}
int main(void){uint8_t b[1024],old[1024];memset(b,0xa5,sizeof b);size_t n=0;assert(lar_build(b,sizeof b,LAT_GPA,&n)&&validate(b,n));for(unsigned i=LAR_SIZE;i<sizeof b;i++)assert(b[i]==0xa5);
 unsigned starts[]={0,64,128,256,576},lens[]={36,52,74,276,136};for(unsigned t=0;t<5;t++)for(unsigned j=0;j<lens[t];j++){uint8_t copy[LAR_SIZE];memcpy(copy,b,LAR_SIZE);copy[starts[t]+j]^=1;assert(!validate(copy,LAR_SIZE));}
 memcpy(old,b,sizeof b);size_t caps[]={0,LAR_SIZE-1,LAT_REGION_SIZE+1,SIZE_MAX};for(unsigned i=0;i<4;i++){n=55;assert(!lar_build(b,caps[i],LAT_GPA,&n)&&n==55&&!memcmp(old,b,sizeof b));}
 assert(!lar_build(b,sizeof b,LAT_GPA+16,&n));assert(!lar_build(0,sizeof b,LAT_GPA,&n));assert(!lar_build(b,sizeof b,LAT_GPA,0));puts("hardware-reduced ACPI description host tests PASS");}
