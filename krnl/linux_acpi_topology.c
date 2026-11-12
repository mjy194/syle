#include "linux_acpi_topology.h"
static void put(uint8_t*p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(v>>(i*8));}
static void bytes(uint8_t*p,const char*s,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)s[i];}
static void checksum(uint8_t*p,unsigned n,unsigned index){unsigned sum=0;for(unsigned i=0;i<n;i++)sum+=p[i];p[index]=(uint8_t)(0u-sum);}
static void header(uint8_t*p,const char*sig,unsigned n,unsigned rev){bytes(p,sig,4);put(p+4,n,4);p[8]=(uint8_t)rev;bytes(p+10,"SYLE  ",6);bytes(p+16,"SYLETOPO",8);put(p+24,1,4);bytes(p+28,"SYLE",4);put(p+32,1,4);}
bool lat_build(void*out,size_t cap,uint64_t base,size_t*size){
 if(!out||!size||base!=LAT_GPA||(base&15)||cap<LAT_SIZE||cap>LAT_REGION_SIZE||base>UINT64_MAX-cap)return false;
 uint8_t b[LAT_SIZE]={0};
 bytes(b,"RSD PTR ",8);bytes(b+9,"SYLE  ",6);b[15]=2;put(b+20,36,4);put(b+24,base+64,8);
 checksum(b,20,8);checksum(b,36,32);
 uint8_t*x=b+64;header(x,"XSDT",44,1);put(x+36,base+128,8);checksum(x,44,9);
 uint8_t*m=b+128;header(m,"APIC",74,5);put(m+36,0xfee00000,4);put(m+40,0,4);
 /* Type0 processor local APIC: UID0, APIC ID0, enabled. */
 m[44]=0;m[45]=8;m[46]=0;m[47]=0;put(m+48,1,4);
 /* IOAPIC ID0, fixed address and GSI base0. */
 m[52]=1;m[53]=12;m[54]=0;put(m+56,0xfec00000,4);put(m+60,0,4);
 /* ISA source IRQ4 -> GSI4, explicit active-high / level. */
 m[64]=2;m[65]=10;m[66]=0;m[67]=4;put(m+68,4,4);put(m+72,0x0d,2);
 checksum(m,74,9);
 uint8_t*d=out;for(unsigned i=0;i<LAT_SIZE;i++)d[i]=b[i];*size=LAT_SIZE;return true;
}
