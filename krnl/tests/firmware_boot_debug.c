#include "../firmware_boot_debug.h"
#include <assert.h>
#include <stdio.h>
struct sink {unsigned count,fail;uint32_t field[4];uint64_t value[4];struct fbd_bank*bank;bool reenter;};
static bool write(void*x,uint32_t f,uint64_t v){struct sink*s=x;unsigned i=s->count++;s->field[i]=f;s->value[i]=v;if(s->reenter){s->reenter=false;uint64_t r[16]={0};uint8_t b[]={15,35,0xc0};assert(!fbd_apply(s->bank,r,0x1000,2,0,3,b,3,x,write));}return s->count!=s->fail;}
int main(void){struct fbd_bank d={0};uint64_t r[16]={0};uint8_t b[]={15,35,0xc2};struct sink s={0};r[2]=0x1234;assert(fbd_apply(&d,r,0x1000,2,0x200,3,b,3,&s,write));assert(d.address[0]==0x1234&&s.count==1&&s.value[0]==0x1003);
 b[1]=0x21;r[2]=0;s=(struct sink){0};assert(fbd_apply(&d,r,0x1000,2,0x210,3,b,3,&s,write)&&r[2]==0x1234);
 b[1]=0x23;r[2]=0x800000000000;assert(!fbd_apply(&d,r,0x1000,2,0x200,3,b,3,&s,write));b[2]=0xfa;r[2]=1;assert(!fbd_apply(&d,r,0x1000,2,0x207,3,b,3,&s,write));r[2]=0x400;s=(struct sink){0};assert(fbd_apply(&d,r,0x1000,2,0x207,3,b,3,&s,write));
 b[1]=0x21;b[2]=0xf4;s=(struct sink){0};assert(fbd_apply(&d,r,0x1000,0x10002,0x416,3,b,3,&s,write));assert(r[4]==0xffff0ff0&&s.count==3&&s.field[0]==0x6820&&s.field[1]==0x681c&&s.field[2]==0x681e);
 uint8_t rex[]={0x41,15,35,0xc0};r[8]=0xffff800000001000;s=(struct sink){0};assert(fbd_apply(&d,r,0x1000,2,0x800,4,rex,4,&s,write));assert(d.address[0]==r[8]);assert(!fbd_apply(&d,r,0x1000,2,0x700,4,rex,4,&s,write));assert(!fbd_apply(&d,r,0x1000,0x102,0x800,4,rex,4,&s,write));
 for(unsigned fail=1;fail<=3;fail++){struct fbd_bank n={0};s=(struct sink){.fail=fail};assert(!fbd_apply(&n,r,0x1000,0x10002,0x416,3,b,3,&s,write)&&n.failed);assert(!fbd_apply(&n,r,0x1000,2,0x416,3,b,3,&s,write));}
 d=(struct fbd_bank){0};s=(struct sink){.bank=&d,.reenter=true};assert(!fbd_apply(&d,r,0x1000,2,0x416,3,b,3,&s,write)&&d.failed);puts("disabled debug bank encoding, RF, RSP, partial failure, reentry PASS");}
