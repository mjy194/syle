#include "nested_operand.h"
/* Intel SDM Vol.3C VMX instruction reference: VMREAD r/m64,r64 (MR),
 * VMWRITE r64,r/m64 (RM). In both, reg encodes the field and r/m the data.
 * Operand size is fixed 64 in 64-bit mode, independent of REX.W.
 * Reference: 326019-sdm-vol-3c.pdf, VMREAD/VMWRITE instruction entries. */
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
bool nested_decode_operand(const uint8_t *code,size_t available,uint64_t rip,
                           const uint64_t gpr[16],uint32_t expected_reason,
                           struct nested_operand *out) {
    if(!code || !gpr || !out || !canonical48(rip)) return false;
    size_t p=0;
    uint8_t b,rex=0,prefix=0,opcode,modrm;
    if(!byte(code,available,&p,&b)) return false;
    if(b==0x66 || b==0xf3) {
        prefix=b;
        if(!byte(code,available,&p,&b)) return false;
    }
    if(b>=0x40 && b<=0x4f) {
        rex=b;
        if(!byte(code,available,&p,&b)) return false;
    }
    if(b!=0x0f || !byte(code,available,&p,&opcode)) return false;
    bool invept=opcode==0x38;
    if(invept) {
        uint8_t third;
        if(prefix!=0x66 || !byte(code,available,&p,&third) || third!=0x80)
            return false;
    }
    if(!byte(code,available,&p,&modrm)) return false;
    struct nested_operand result={.regA=0xff,.regB=0xff};
    unsigned reason=0,mod=modrm>>6,rm=modrm&7,group=(modrm>>3)&7;
    if(invept && mod!=3) {
        result.op=NVMX_INVEPT;
        result.memory=true;
        result.regB=group|((rex&4)?8:0);
        reason=50;
    } else if(opcode==0x01 && !prefix) {
        if(modrm==0xc2) { result.op=NVMX_LAUNCH;reason=20; }
        else if(modrm==0xc3) { result.op=NVMX_RESUME;reason=24; }
        else if(modrm==0xc4) { result.op=NVMX_OFF;reason=26; }
        else return false;
    } else if((opcode==0x78 || opcode==0x79) && !prefix && mod==3) {
        result.op=opcode==0x78?NVMX_READ:NVMX_WRITE;
        reason=opcode==0x78?23:25;
        result.regA=rm|((rex&1)?8:0);
        result.regB=group|((rex&4)?8:0);
    } else if(opcode==0xc7 && mod!=3) {
        if(group==6) {
            if(prefix==0xf3) {result.op=NVMX_ON;reason=27;}
            else if(prefix==0x66) {result.op=NVMX_CLEAR;reason=19;}
            else {result.op=NVMX_PTRLD;reason=21;}
        } else if(group==7 && !prefix) {result.op=NVMX_PTRST;reason=22;}
        else return false;
        result.memory=true;
    } else return false;
    if(reason!=expected_reason) return false;
    if(!result.memory) {
        uint64_t next;
        if(!add(rip,p,&next) || !canonical48(next)) return false;
        result.length=(uint8_t)p;
        *out=result;
        return true;
    }
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
    unsigned width=invept?16:8;
    if(!canonical48(ea) || !add(ea,width-1,&last) || !canonical48(last) ||
       (ea&0xfff)>4096-width) return false;
    result.ea=ea;
    result.length=(uint8_t)p;
    *out=result;
    return true;
}
