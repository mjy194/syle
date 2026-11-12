#ifndef SYLE_NESTED_EPT_ATOMIC_FORMAT_H
#define SYLE_NESTED_EPT_ATOMIC_FORMAT_H
#include <stdbool.h>
#include <stdint.h>
/* Present-word decoder shared by walk, durable-trace validation and stopped
 * maintenance. depth is 0=PML4 .. 3=PT; shift==0 denotes a table pointer.
 * This deliberately supports neither execute-only nor ignored PT bit 7.
 * Unsupported format is never authority to synthesize architectural reason49.
 * Callers handle nonpresent words before decoding their ignored payload. */
static inline bool nea_decode_policy(uint64_t raw,unsigned depth,uint64_t mask,
 bool ipat,bool allow_2m,bool allow_1g,bool fixed_uc,unsigned *shift) {
 if((fixed_uc&&(!ipat||allow_2m||allow_1g))||depth>3||!(raw&1))return false;
 bool large=(raw&0x80)!=0;
 if(large&&!((allow_2m&&depth==2&&mask>=UINT64_C(0x1ff000))||
              (allow_1g&&depth==1&&mask>=UINT64_C(0x3ffff000))))return false;
 bool leaf=depth==3||large;
 uint64_t allowed=mask|7|0x100|(leaf?(UINT64_C(0x238)|(ipat?UINT64_C(0x40):0)):0)|(large?0x80:0);
 unsigned type=(raw>>3)&7;
 if((raw&~allowed)||(leaf&&((type!=6&&!(fixed_uc&&type==0&&!(raw&4)))||(ipat&&!(raw&0x40)))))return false;
 unsigned bits=large?(depth==1?30:21):leaf?12:0;
 if(bits&&((raw&mask)&((UINT64_C(1)<<bits)-1)))return false;
 *shift=bits;return true;
}
/* All earlier entry points remain exactly WB-only. */
static inline bool nea_decode(uint64_t raw,unsigned depth,uint64_t mask,
 bool ipat,bool allow_2m,bool allow_1g,unsigned *shift) {
 return nea_decode_policy(raw,depth,mask,ipat,allow_2m,allow_1g,false,shift);
}
static inline uint64_t nea_leaf_page(uint64_t raw,uint64_t gpa,uint64_t mask,unsigned shift) {
 return (raw&mask)+(gpa&(((UINT64_C(1)<<shift)-1)&~UINT64_C(4095)));
}
#endif
