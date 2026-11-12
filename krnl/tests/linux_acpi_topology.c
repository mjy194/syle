#include "../linux_acpi_topology.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint64_t get(const unsigned char*p,unsigned n){uint64_t x=0;for(unsigned i=0;i<n;i++)x|=(uint64_t)p[i]<<(i*8);return x;}
static bool sum(const unsigned char*p,size_t n){unsigned x=0;for(size_t i=0;i<n;i++)x+=p[i];return !(x&255);}
/* Independent bounded parser verifies cross-pointers and each subtable. */
static bool valid(const unsigned char*b,size_t n){
 if(n!=202||memcmp(b,"RSD PTR ",8)||b[15]!=2||get(b+20,4)!=36||get(b+16,4)||!sum(b,20)||!sum(b,36))return false;
 uint64_t xp=get(b+24,8);if(xp!=LAT_GPA+64)return false;const unsigned char*x=b+64;
 if(memcmp(x,"XSDT",4)||get(x+4,4)!=44||!sum(x,44)||get(x+36,8)!=LAT_GPA+128)return false;
 const unsigned char*m=b+128;if(memcmp(m,"APIC",4)||get(m+4,4)!=74||!sum(m,74)||get(m+36,4)!=0xfee00000||get(m+40,4))return false;
 if(m[44]!=0||m[45]!=8||m[46]||m[47]||get(m+48,4)!=1)return false;
 if(m[52]!=1||m[53]!=12||m[54]!=0||m[55]||get(m+56,4)!=0xfec00000||get(m+60,4))return false;
 return m[64]==2&&m[65]==10&&!m[66]&&m[67]==4&&get(m+68,4)==4&&get(m+72,2)==0xd;
}
int main(void){unsigned char b[256],old[256];memset(b,0xa5,sizeof b);size_t n=99;
 assert(lat_build(b,sizeof b,LAT_GPA,&n)&&n==LAT_SIZE&&valid(b,n));for(unsigned i=LAT_SIZE;i<sizeof b;i++)assert(b[i]==0xa5);
 for(unsigned i=0;i<LAT_SIZE;i++){unsigned char copy[256];memcpy(copy,b,sizeof b);copy[i]^=1;if(i<36||(i>=64&&i<108)||i>=128)assert(!valid(copy,n));}
 memcpy(old,b,sizeof b);uint64_t bad[]={0,LAT_GPA+1,LAT_GPA+16,UINT64_MAX-1};for(unsigned i=0;i<4;i++){n=99;assert(!lat_build(b,sizeof b,bad[i],&n)&&n==99&&!memcmp(b,old,sizeof b));}
 size_t caps[]={0,LAT_SIZE-1,LAT_REGION_SIZE+1,SIZE_MAX};for(unsigned i=0;i<4;i++){n=99;assert(!lat_build(b,caps[i],LAT_GPA,&n)&&n==99&&!memcmp(b,old,sizeof b));}
 assert(!lat_build(0,sizeof b,LAT_GPA,&n));assert(!lat_build(b,sizeof b,LAT_GPA,0));
 assert(LAT_GPA>=0x9f000&&LAT_GPA+LAT_REGION_SIZE<=0x100000); /* Existing reserved E820 interval. */
 puts("ACPI topology fragment host tests PASS");}
