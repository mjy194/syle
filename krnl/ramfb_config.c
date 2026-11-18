#include "ramfb_config.h"
static uint32_t be32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t be64(const uint8_t*p){return ((uint64_t)be32(p)<<32)|be32(p+4);}
bool ramfb_parse(const uint8_t*p,size_t n,uint64_t base,uint64_t size,struct ramfb_config*out){
 if(!p||!out||n!=28||!size||base>UINT64_MAX-size)return false;
 struct ramfb_config c={.address=be64(p),.width=be32(p+16),.height=be32(p+20),.stride=be32(p+24)};
 if(be32(p+8)!=UINT32_C(0x34325258)||be32(p+12)||c.width<16||c.width>1024||c.height<16||c.height>768)return false;
 uint64_t row=(uint64_t)c.width*4,rows=(uint64_t)c.height-1;
 if(c.stride<row||rows>(UINT64_MAX-row)/c.stride)return false;
 c.span=rows*c.stride+row;
 if(c.address<base||c.address-base>=size||c.span>size-(c.address-base))return false;
 *out=c;return true;
}
