#include "firmware_boot_debug.h"
static bool canonical(uint64_t v){return (v>>47)==0||(v>>47)==0x1ffff;}
bool fbd_apply_qualified(struct fbd_bank*d,uint64_t r[16],uint64_t rip,uint64_t flags,
 uint64_t q,unsigned len,void*c,bool(*w)(void*,uint32_t,uint64_t)){
 if(!d)return false;
 if(d->busy){d->failed=true;return false;}
 if(d->failed||!r||!w||!(flags&2)||(flags&~UINT64_C(0x250ed7))||(flags&0x100)||
 (q&~UINT64_C(0xf17))||!canonical(rip)||rip>UINT64_MAX-len||!canonical(rip+len))return false;
 unsigned dr=q&7,reg=(q>>8)&15;bool input=(q&16)!=0;
 if((len!=3&&len!=4)||dr==4||dr==5)return false;
 for(unsigned j=0;j<4;j++)if(!canonical(d->address[j]))return false;
 uint64_t v=input?(dr<4?d->address[dr]:dr==6?UINT64_C(0xffff0ff0):0x400):r[reg];
 if(!input&&((dr<4&&!canonical(v))||(dr==6&&v!=0&&v!=0xffff0ff0)||(dr==7&&v!=0&&v!=0x400)))return false;
 d->busy=true;
 if(((flags&0x10000)&&(!w(c,0x6820,flags&~UINT64_C(0x10000))||d->failed))||
 (input&&reg==4&&(!w(c,0x681c,v)||d->failed))||!w(c,0x681e,rip+len)||d->failed){d->failed=true;d->busy=false;return false;}
 if(input)r[reg]=v;else if(dr<4)d->address[dr]=v;
 d->busy=false;return true;
}

bool fbd_apply(struct fbd_bank*d,uint64_t r[16],uint64_t rip,uint64_t flags,
 uint64_t q,unsigned len,const uint8_t*b,size_t n,void*c,bool(*w)(void*,uint32_t,uint64_t)){
 if(!b||!d)return false;
 if(d->busy){d->failed=true;return false;}
 unsigned dr=q&7,reg=(q>>8)&15,i=0;bool input=(q&16)!=0;
 uint8_t rex=0;if(n&&b[0]>=0x40&&b[0]<=0x4f){rex=b[0];i++;if(rex&6)return false;}
 if(len!=i+3||n<len||b[i]!=0x0f||b[i+1]!=(input?0x21:0x23)||
 b[i+2]!=(0xc0|(dr<<3)|(reg&7))||((rex&1)!=0)!=(reg>=8))return false;
 return fbd_apply_qualified(d,r,rip,flags,q,len,c,w);
}
