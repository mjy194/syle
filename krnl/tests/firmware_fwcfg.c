#include "../firmware_fwcfg.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void select_item(struct ffc_state*s,uint16_t key){uint64_t v=0xaabbccdd12340000ULL|key;assert(ffc_io(s,0x510,2,false,&v)&&v==(0xaabbccdd12340000ULL|key)&&s->cursor==0&&s->selector==key);}
static uint8_t read_byte(struct ffc_state*s){uint64_t v=0x123456789abcdeffULL;assert(ffc_io(s,0x511,1,true,&v));assert((v&~255ULL)==0x123456789abcde00ULL);return (uint8_t)v;}
static void reject(struct ffc_state*s,uint16_t port,unsigned width,bool input,uint64_t v){struct ffc_state old=*s;uint64_t saved=v;assert(!ffc_io(s,port,width,input,&v)&&!memcmp(s,&old,sizeof old)&&v==saved);}
int main(void){
 struct ffc_state s={0};uint64_t v=0;assert(!ffc_init(&s,0,1)&&!ffc_init(&s,128u*1024u*1024u,0)&&!ffc_init(&s,128u*1024u*1024u,2)&&!s.ready);assert(!ffc_io(&s,0x511,1,true,&v));assert(ffc_init(&s,128u*1024u*1024u,1)&&!ffc_init(&s,128u*1024u*1024u,1)&&!ffc_init(0,128u*1024u*1024u,1));
 assert(read_byte(&s)=='Q'&&read_byte(&s)=='E');select_item(&s,0);assert(read_byte(&s)=='Q');
 const uint16_t keys[]={0,1,3,5,0xf,0x19};
 const uint8_t expected[][8]={{'Q','E','M','U'},{1,0,0,0},{0,0,0,8,0,0,0,0},{1,0},{1,0},{0,0,0,0}};
 const unsigned lengths[]={4,4,8,2,2,4};
 for(unsigned i=0;i<6;i++){select_item(&s,keys[i]);for(unsigned j=0;j<lengths[i];j++)assert(read_byte(&s)==expected[i][j]);for(unsigned j=0;j<20;j++)assert(!read_byte(&s));assert(s.cursor==lengths[i]+20);}
 /* All other selector bits/keys reject without resetting existing stream. */
 select_item(&s,0);assert(read_byte(&s)=='Q');
 for(unsigned key=0;key<65536;key++){
  bool valid=false;for(unsigned i=0;i<6;i++)if(key==keys[i])valid=true;
  if(!valid)reject(&s,0x510,2,false,key);
 }
 assert(read_byte(&s)=='E');
 unsigned widths[]={0,1,2,3,4,8,UINT32_MAX};
 for(unsigned i=0;i<sizeof widths/sizeof*widths;i++){
  if(widths[i]!=2)reject(&s,0x510,widths[i],false,0);
  reject(&s,0x510,widths[i],true,0);
  if(widths[i]!=1)reject(&s,0x511,widths[i],true,0);
  reject(&s,0x511,widths[i],false,0);
  reject(&s,0x514,widths[i],false,0);reject(&s,0x514,widths[i],true,0);
 }
 reject(&s,0x518,4,false,0);reject(&s,0x512,1,true,0);reject(&s,0xffff,1,true,0);
 assert(!ffc_io(&s,0x511,1,true,0)&&!ffc_io(0,0x511,1,true,&v));
 /* Copied models advance independently and carry no caller data pointers. */
 select_item(&s,0);struct ffc_state copy=s;assert(read_byte(&copy)=='Q'&&s.cursor==0);assert(read_byte(&s)=='Q'&&read_byte(&s)=='E'&&copy.cursor==1);
 select_item(&copy,3);assert(s.selector==0);select_item(&s,0);assert(read_byte(&s)=='Q');
 s.cursor=UINT64_MAX-1;assert(!read_byte(&s)&&s.cursor==UINT64_MAX);reject(&s,0x511,1,true,0xa5);select_item(&s,0);assert(read_byte(&s)=='Q');
 s.selector=0x4000;reject(&s,0x511,1,true,0xa5);select_item(&s,0);
 /* Exact scalar VMX qualification, no REP/INSB pretending to be scalar. */
 v=1;assert(ffc_exit(&s,(0x510ULL<<16)|1,&v)&&s.selector==1);
 v=~0ULL;assert(ffc_exit(&s,(0x511ULL<<16)|8,&v)&&(uint8_t)v==1);
 uint64_t bad[]={1ULL<<63,1ULL<<32,1ULL<<15,1ULL<<7,1ULL<<5,1ULL<<4};
 for(unsigned i=0;i<sizeof bad/sizeof*bad;i++){struct ffc_state old=s;v=0xaabb;assert(!ffc_exit(&s,(0x511ULL<<16)|8|bad[i],&v)&&!memcmp(&s,&old,sizeof s)&&v==0xaabb);}
 puts("firmware fw_cfg: explicit bytes, reset/tail, exhaustive unknown keys, widths, isolation PASS");return 0;
}
