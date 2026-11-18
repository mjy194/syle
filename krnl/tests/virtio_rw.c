#define _POSIX_C_SOURCE 200809L
#include "../virtio_blk/rw_transport.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static uint8_t ram[65536];static FILE*file;static unsigned flushes,stores;static bool fail_flush,fail_write;
static bool admit(void*c,uint64_t a,size_t n,bool w){(void)c;(void)w;return a<sizeof ram&&n<=sizeof ram-a;}
static bool rd(void*c,uint64_t a,void*b,size_t n){if(!admit(c,a,n,0))return false;memcpy(b,ram+a,n);return true;}
static bool wr(void*c,uint64_t a,const void*b,size_t n){if(!admit(c,a,n,1))return false;memcpy(ram+a,b,n);return true;}
static void barrier(void*c){(void)c;__atomic_thread_fence(__ATOMIC_RELEASE);}
static bool diskread(void*c,uint64_t s,uint8_t b[512]){(void)c;return !fseek(file,(long)s*512,SEEK_SET)&&fread(b,1,512,file)==512;}
static bool diskwrite(void*c,uint64_t s,const uint8_t b[512]){(void)c;stores++;return !fail_write&&!fseek(file,(long)s*512,SEEK_SET)&&fwrite(b,1,512,file)==512;}
static bool flush(void*c){(void)c;flushes++;return !fail_flush&&!fflush(file)&&!fsync(fileno(file));}
static void put(uint64_t a,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)ram[a+i]=v>>(i*8);}
static uint64_t get(uint64_t a,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)ram[a+i]<<(i*8);return v;}
static void desc(unsigned i,uint64_t a,unsigned len,unsigned flags,unsigned next){put(4096+16*i,a,8);put(4104+16*i,len,4);put(4108+16*i,flags,2);put(4110+16*i,next,2);}
static bool io(struct vrwt*t,unsigned o,unsigned n,uint32_t v){return vrwt_io(t,0xc000+o,n,true,&v);}
static struct vrwt init(void){memset(ram,0,sizeof ram);struct vrwt t;assert(vrwt_init(&t,(struct vro_io){0,admit,rd,wr,barrier},(struct vrw_store){0,16,diskread,diskwrite,flush}));uint32_t v=0xc001;assert(vrwt_config(&t,16,4,1,&v));v=5;assert(vrwt_config(&t,4,2,1,&v));assert(io(&t,18,1,3)&&io(&t,4,4,512)&&io(&t,8,4,1)&&io(&t,18,1,7));fail_flush=fail_write=false;return t;}
static void request(unsigned head,unsigned kind,uint64_t sector,unsigned slot){unsigned h=16384+head*32,data=24576+head*1024,status=49152+head;put(h,kind,4);put(h+8,sector,8);desc(head,h,16,1,head+1);if(kind==4)desc(head+1,status,1,2,0);else{desc(head+1,data,17,kind==1?1:3,head+2);desc(head+2,data+17,495,kind==1?1:3,head+3);desc(head+3,status,1,2,0);}ram[status]=99;put(6148+slot*2,head,2);put(6146,slot+1,2);}
int main(void){char name[]="/tmp/syle-rw-test.XXXXXX";int fd=mkstemp(name);assert(fd>=0);file=fdopen(fd,"w+b");uint8_t zero[8192]={0};assert(fwrite(zero,1,sizeof zero,file)==sizeof zero);assert(!fflush(file));
 struct vrwt t=init();request(0,1,15,0);for(unsigned i=0;i<512;i++)ram[24576+i]=i*13;assert(io(&t,16,2,0));assert(ram[49152]==0&&get(8194,2)==1&&flushes==1&&stores==1&&vrwt_irq(&t));assert(!fclose(file));file=fopen(name,"r+b");uint8_t saved[512];assert(diskread(0,15,saved)&&!memcmp(saved,ram+24576,512));uint32_t v=0;assert(vrwt_io(&t,0xc013,1,false,&v)&&v==1&&!vrwt_irq(&t));
 t=init();request(0,0,15,0);request(4,4,0,1);assert(io(&t,16,2,0)&&get(8194,2)==2&&!memcmp(ram+24576,saved,512)&&ram[49156]==0);
 t=init();request(0,1,16,0);unsigned before=stores;assert(io(&t,16,2,0)&&ram[49152]==1&&stores==before);
 t=init();request(0,1,0,0);fail_flush=true;assert(!io(&t,16,2,0)&&t.failed&&get(8194,2)==0&&ram[49152]==99);assert(!io(&t,18,1,0));
 t=init();request(0,1,0,0);fail_write=true;assert(!io(&t,16,2,0)&&t.failed&&get(8194,2)==0);
 t=init();request(0,0,0,0);desc(1,4096,512,3,3);assert(!io(&t,16,2,0)&&ram[49152]==99);
 t=init();request(0,0,0,0);put(6146,129,2);assert(!io(&t,16,2,0));
 assert(!fclose(file));unlink(name);puts("RW virtqueue: split scatter, batched IN/FLUSH, durable reopen, bounds, IRQ ack, partial failure PASS");}
