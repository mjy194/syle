#include "linux_acpi_reduced.h"
static const uint8_t dsdt[]={
#include "linux_acpi_reduced_dsdt.inc"
};
_Static_assert(sizeof dsdt==136,"DSDT layout changed: review blob and tests");
static void put(uint8_t*p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=v>>(8*i);}
static void text(uint8_t*p,const char*s,unsigned n){for(unsigned i=0;i<n;i++)p[i]=s[i];}
static void sum(uint8_t*p,unsigned n,unsigned field){p[field]=0;unsigned s=0;for(unsigned i=0;i<n;i++)s+=p[i];p[field]=(uint8_t)(0-s);}
static void head(uint8_t*p,const char*s,unsigned len,unsigned rev){text(p,s,4);put(p+4,len,4);p[8]=rev;text(p+10,"SYLE  ",6);text(p+16,"SYLEREDU",8);put(p+24,1,4);text(p+28,"SYLE",4);put(p+32,1,4);}
bool lar_build(void*out,size_t cap,uint64_t base,size_t*size){
 if(!out||!size||base!=LAT_GPA||cap<LAR_SIZE||cap>LAT_REGION_SIZE||base>UINT64_MAX-cap)return false;
 uint8_t b[LAR_SIZE]={0};size_t n;if(!lat_build(b,LAT_SIZE,base,&n)||n!=LAT_SIZE)return false;
 uint8_t*x=b+64;head(x,"XSDT",52,1);put(x+36,base+256,8);put(x+44,base+128,8);sum(x,52,9);
 uint8_t*f=b+256;head(f,"FACP",276,6);put(f+40,base+576,4);put(f+140,base+576,8);
 /* No legacy devices/8042 claimed. MSI not supported; no CMOS RTC. */
 put(f+109,0x28,2);put(f+112,(1u<<20)|(1u<<4)|(1u<<5),4);f[131]=5;
 /* All FACS, PM, SCI, reset, sleep GAS remain zero. No fake IO addresses. */
 text(f+268,"SYLE    ",8);sum(f,276,9);
 for(unsigned i=0;i<sizeof dsdt;i++)b[576+i]=dsdt[i];
 uint8_t*d=out;for(unsigned i=0;i<LAR_SIZE;i++)d[i]=b[i];*size=LAR_SIZE;return true;
}
