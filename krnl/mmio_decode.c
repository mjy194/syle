#include "mmio_decode.h"

static bool canonical48(uint64_t address) {
    return address<=UINT64_C(0x00007fffffffffff) ||
           address>=UINT64_C(0xffff800000000000);
}
static bool add(uint64_t a,uint64_t b,uint64_t *result) {
    if(b>UINT64_MAX-a) return false;
    *result=a+b;
    return true;
}
static bool byte(const uint8_t *code,size_t available,size_t *position,uint8_t *value) {
    if(*position>=available || *position>=15) return false;
    *value=code[(*position)++];
    return true;
}
static bool displacement(const uint8_t *code,size_t available,size_t *position,
                          unsigned bytes,int64_t *value) {
    uint32_t raw=0;
    for(unsigned i=0;i<bytes;i++) {
        uint8_t b;
        if(!byte(code,available,position,&b)) return false;
        raw|=(uint32_t)b<<(8*i);
    }
    /* Explicit sign extension avoids implementation-defined unsigned->signed. */
    if(bytes==1) *value=raw<0x80?raw:(int64_t)raw-0x100;
    else *value=raw<UINT32_C(0x80000000)?raw:(int64_t)raw-INT64_C(0x100000000);
    return true;
}
bool mmio_decode_mov32(const uint8_t *code,size_t available,uint64_t rip,
                       const uint64_t gpr[16],struct mmio_mov32 *out) {
    if(!code || !gpr || !out || !canonical48(rip)) return false;
    size_t p=0;
    uint8_t opcode,rex=0,modrm;
    if(!byte(code,available,&p,&opcode)) return false;
    if(opcode>=0x40 && opcode<=0x4f) {
        rex=opcode;
        if((rex&8) || !byte(code,available,&p,&opcode)) return false;
    }
    if(opcode!=0x89 && opcode!=0x8b) return false;
    if(!byte(code,available,&p,&modrm)) return false;
    unsigned mod=modrm>>6,rm=modrm&7;
    if(mod==3) return false;
    unsigned reg=((modrm>>3)&7)|((rex&4)?8:0);
    unsigned disp_bytes=mod==1?1:mod==2?4:0;
    bool rip_relative=false,no_base=false;
    uint64_t base=0,index=0;
    if(rm==4) {
        uint8_t sib;
        if(!byte(code,available,&p,&sib)) return false;
        unsigned index_code=(sib>>3)&7,base_code=sib&7,scale=sib>>6;
        /* Index=4 suppresses the index only without REX.X. */
        if(index_code!=4 || (rex&2)) {
            uint64_t raw=gpr[index_code|((rex&2)?8:0)];
            if(raw>(UINT64_MAX>>scale)) return false;
            index=raw<<scale;
        }
        /* Mod=00/base=5 is base-less, even if REX.B is set. */
        if(mod==0 && base_code==5) {
            no_base=true;disp_bytes=4;
        } else base=gpr[base_code|((rex&1)?8:0)];
    } else if(mod==0 && rm==5) {
        /* This remains RIP-relative even with REX.B. */
        rip_relative=true;disp_bytes=4;
    } else base=gpr[rm|((rex&1)?8:0)];

    int64_t disp=0;
    if(disp_bytes && !displacement(code,available,&p,disp_bytes,&disp)) return false;
    uint64_t next_rip;
    if(!add(rip,p,&next_rip) || !canonical48(next_rip)) return false;
    if(rip_relative) base=next_rip;
    uint64_t ea;
    if(!add(base,index,&ea)) return false;
    if(disp>=0) {
        if(!add(ea,(uint64_t)disp,&ea)) return false;
    } else {
        uint64_t magnitude=(uint64_t)(-disp); /* disp fits signed 32 bits */
        if(ea>=magnitude) ea-=magnitude;
        else {
            /* A base-less signed disp32 can legitimately form a negative
             * canonical address; do not mistake that sign extension for
             * overflow. Other subtraction underflow is unsupported. */
            if(!no_base) return false;
            uint64_t negative=magnitude-ea;
            ea=UINT64_MAX-(negative-1);
        }
    }
    uint64_t last;
    if(!canonical48(ea) || !add(ea,3,&last) || !canonical48(last) ||
       (ea&0xfff)>0xffc) return false;
    *out=(struct mmio_mov32){.ea=ea,.length=(uint8_t)p,.reg=(uint8_t)reg,
                            .write=opcode==0x89};
    return true;
}
