#include "guest_mmio_mov.h"
static bool canonical(uint64_t a) {
    return a<=UINT64_C(0x7fffffffffff) || a>=UINT64_C(0xffff800000000000);
}
static bool get(const uint8_t *b,size_t n,size_t *p,uint8_t *v) {
    if(*p>=n || *p>=15) return false;
    *v=b[(*p)++];return true;
}
static bool raw(const uint8_t *b,size_t n,size_t *p,unsigned bytes,uint64_t *v) {
    *v=0;
    for(unsigned i=0;i<bytes;i++) {uint8_t c;if(!get(b,n,p,&c)) return false;*v|=(uint64_t)c<<(8*i);}
    return true;
}
static uint64_t mask(unsigned width) {
    return width==8?UINT64_MAX:(UINT64_C(1)<<(width*8))-1;
}
bool guest_mmio_mov_decode(const uint8_t *b,size_t n,uint64_t rip,
                          const uint64_t gpr[16],struct guest_mmio_mov *out) {
    if(!b || !gpr || !out || !canonical(rip)) return false;
    size_t p=0;uint8_t op,rex=0;bool operand16=false,addr32=false;
    if(!get(b,n,&p,&op)) return false;
    while(op==0x66 || op==0x67) {
        if(op==0x66) {if(operand16) return false;operand16=true;}
        else {if(addr32) return false;addr32=true;}
        if(!get(b,n,&p,&op)) return false;
    }
    if(op>=0x40 && op<=0x4f) {rex=op;if(!get(b,n,&p,&op)) return false;}
    bool immediate=op==0xc6 || op==0xc7;
    if(op!=0x88 && op!=0x89 && op!=0x8a && op!=0x8b && !immediate) return false;
    unsigned width=(op==0x88 || op==0x8a || op==0xc6)?1:(rex&8)?8:operand16?2:4;
    uint8_t mr;if(!get(b,n,&p,&mr)) return false;
    unsigned mod=mr>>6,rm=mr&7,reg=((mr>>3)&7)|((rex&4)?8:0);
    if(mod==3 || (immediate && reg!=0)) return false;
    unsigned shift=0;
    if(width==1 && !rex && reg>=4) {reg-=4;shift=8;}
    uint64_t base=0,index=0;bool relative=false;
    unsigned dispbytes=mod==1?1:mod==2?4:0;
    if(rm==4) {
        uint8_t sib;if(!get(b,n,&p,&sib)) return false;
        unsigned ix=(sib>>3)&7,ba=sib&7,scale=sib>>6;
        if(ix!=4 || (rex&2)) index=gpr[ix|((rex&2)?8:0)]<<scale;
        if(mod==0 && ba==5) dispbytes=4;
        else base=gpr[ba|((rex&1)?8:0)];
    } else if(mod==0 && rm==5) {relative=true;dispbytes=4;}
    else base=gpr[rm|((rex&1)?8:0)];
    uint64_t disp=0;
    if(dispbytes) {
        if(!raw(b,n,&p,dispbytes,&disp)) return false;
        if(disp&(UINT64_C(1)<<(dispbytes*8-1))) disp|=~mask(dispbytes);
    }
    uint64_t store=0;
    if(immediate) {
        unsigned bytes=width==8?4:width;
        if(!raw(b,n,&p,bytes,&store)) return false;
        if(width==8 && (store&UINT64_C(0x80000000))) store|=UINT64_C(0xffffffff00000000);
    } else store=(gpr[reg]>>shift)&mask(width);
    if(rip>UINT64_MAX-p || !canonical(rip+p)) return false;
    uint64_t next=rip+p;
    if(relative) base=next;
    /* Architectural address calculation is modular. For address-size32,
     * truncate the complete sum; otherwise unsigned arithmetic wraps64. */
    uint64_t ea=base+index+disp;
    if(addr32) ea=(uint32_t)ea;
    if(!canonical(ea) || ea>UINT64_MAX-(width-1) ||
       !canonical(ea+width-1) || (ea&0xfff)>4096-width) return false;
    *out=(struct guest_mmio_mov){.linear=ea,.next_rip=next,.store_value=store,
        .length=(uint8_t)p,.width=(uint8_t)width,.reg=immediate?255:(uint8_t)reg,
        .reg_shift=(uint8_t)shift,.write=immediate||op==0x88||op==0x89,
        .immediate=immediate,.address32=addr32};
    return true;
}
bool guest_mmio_mov_load_value(const struct guest_mmio_mov *m,uint64_t old,
                              uint64_t loaded,uint64_t *out) {
    if(!m || !out || m->write || m->immediate || m->reg>=16 ||
       (m->width!=1 && m->width!=2 && m->width!=4 && m->width!=8) ||
       (m->reg_shift && (m->reg_shift!=8 || m->width!=1 || m->reg>=4))) return false;
    uint64_t bits=mask(m->width),v=loaded&bits;
    if(m->width==1 || m->width==2) v=(old&~(bits<<m->reg_shift))|(v<<m->reg_shift);
    *out=v;return true;
}
