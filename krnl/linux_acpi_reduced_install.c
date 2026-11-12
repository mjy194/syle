#include "linux_acpi_reduced_install.h"
static uint64_t get(const uint8_t*p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static bool same(const uint8_t*a,const uint8_t*b,size_t n){for(size_t i=0;i<n;i++)if(a[i]!=b[i])return false;return true;}
static bool boot(const struct lai_ops*o,uint64_t ram,uint64_t params,uint64_t pointer){
 if(!o||!o->read||ram!=0x8000000||params!=0x90000)return false;
 uint8_t bp[4096];if(!o->read(o->context,params,bp,sizeof bp)||get(bp+0x70,8)!=pointer||get(bp+0x202,4)!=0x53726448||get(bp+0x206,2)!=0x20f)return false;
 unsigned count=bp[0x1e8];if(!count||count>128)return false;
 bool reserved=false;uint64_t previous=0;
 for(unsigned i=0;i<count;i++){const uint8_t*p=bp+0x2d0+20*i;uint64_t start=get(p,8),len=get(p+8,8);uint32_t type=(uint32_t)get(p+16,4);
 if(!len||start<previous||start>=ram||len>ram-start||!type)return false;
 previous=start+len;
 if(start<LAT_GPA+LAT_REGION_SIZE&&previous>LAT_GPA){if(type!=2||start>LAT_GPA||previous<LAT_GPA+LAT_REGION_SIZE)return false;reserved=true;}}
 return reserved;
}
static bool bytes(const struct lai_ops*o){uint8_t expected[LAR_SIZE],actual[LAR_SIZE];size_t n=0;
 return lar_build(expected,sizeof expected,LAT_GPA,&n)&&n==LAR_SIZE&&o->read(o->context,LAT_GPA,actual,sizeof actual)&&same(expected,actual,sizeof actual);}
bool lar_verify_owned(const struct lai_ops*o,uint64_t ram,uint64_t params){return boot(o,ram,params,LAT_GPA)&&bytes(o);}
bool lar_install(enum lai_state*s,const struct lai_ops*o,uint64_t ram,uint64_t params){
 if(!s||*s!=LAI_FRESH||!o||!o->write||!boot(o,ram,params,0))return false;
 uint8_t blob[LAR_SIZE],pointer[8];size_t n=0;if(!lar_build(blob,sizeof blob,LAT_GPA,&n)||n!=LAR_SIZE)return false;
 for(unsigned i=0;i<8;i++)pointer[i]=(uint8_t)(LAT_GPA>>(8*i));
 *s=LAI_DIRTY;
 if(!o->write(o->context,LAT_GPA,blob,sizeof blob)||!bytes(o))return false;
 /* Publication last; complete RAM verification repeats E820 and table checks. */
 if(!o->write(o->context,params+0x70,pointer,8)||!lar_verify_owned(o,ram,params))return false;
 *s=LAI_INSTALLED;return true;
}
