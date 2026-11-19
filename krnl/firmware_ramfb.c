#include "firmware_ramfb.h"
static uint32_t be32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t be64(const uint8_t*p){return ((uint64_t)be32(p)<<32)|be32(p+4);}
static uint32_t swap(uint32_t v){return __builtin_bswap32(v);}
static bool overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t k){return a<=b?b-a<n:a-b<k;}
bool frfb_io(struct frfb_state*s,struct ffc_state*f,uint16_t port,unsigned width,bool input,uint64_t*rax,void*ctx,
 bool(*read)(void*,uint64_t,void*,unsigned),bool(*write)(void*,uint64_t,const void*,unsigned),bool(*owned)(void*,uint64_t,uint64_t)){
 if(!s||!f||!f->ready||!rax||width!=4||(port!=0x514&&port!=0x518)||!read||!write||!owned)return false;
 if(input){*rax=port==0x514?UINT64_C(0x554d4551):UINT64_C(0x47464320);return true;}
 if(port==0x514){s->address_high=swap((uint32_t)*rax);return true;}
 uint64_t desc=((uint64_t)s->address_high<<32)|swap((uint32_t)*rax);uint8_t d[16],b[64];
 if(!owned(ctx,desc,16)||!read(ctx,desc,d,16))return false;
 uint32_t ctl=be32(d),n=be32(d+4);uint64_t target=be64(d+8);unsigned op=ctl&0xffff;
 if((op&~30u)||((op&2)!=0)+((op&4)!=0)+((op&16)!=0)!=1)return false;
 if(op&8){uint64_t key=ctl>>16;if(!ffc_io(f,0x510,2,false,&key))return false;}
 else if(ctl>>16)return false;
 bool table_read=(op&2)&&f->cpu_count_le[0]==2&&(f->selector==0x19||f->selector==0x21||f->selector==0x22);
 if(n>sizeof b&&(!table_read||n>384))return false;
 if((op&4)==0&&n&&(!owned(ctx,target,n)||overlap(desc,16,target,n)))return false;
 if(op&2){
  for(unsigned done=0;done<n;){
   unsigned count=n-done,page_left=4096-(unsigned)((target+done)&4095);
   if(count>sizeof b)count=sizeof b;
   if(table_read&&count>page_left)count=page_left;
   for(unsigned i=0;i<count;i++){uint64_t v=0;if(!ffc_io(f,0x511,1,true,&v))return false;b[i]=(uint8_t)v;}
   if(!write(ctx,target+done,b,count))return false;
   done+=count;
  }
 }else if(op&4){
  for(unsigned i=0;i<n;i++){uint64_t v=0;if(!ffc_io(f,0x511,1,true,&v))return false;}
 }else{
  struct ramfb_config c;
  if(f->selector!=0x20||f->cursor||n!=28||!read(ctx,target,b,n)||
   !ramfb_parse(b,n,0,FWM_RAM_SIZE,&c)||(c.address&3)||!owned(ctx,c.address,c.span)||
   overlap(c.address,c.span,desc,16)||overlap(c.address,c.span,target,n))return false;
  s->config=c;s->active=true;s->configurations++;f->cursor=n;
 }
 const uint8_t done[4]={0};if(!write(ctx,desc,done,4))return false;
 s->address_high=0;s->transfers++;return true;
}
