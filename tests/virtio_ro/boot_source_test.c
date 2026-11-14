#include "boot_source_candidate.h"
#include "bounded_span.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define IMGSIZE 12288
struct fixture {unsigned char *ram; unsigned char image[IMGSIZE],rd[5000],cmd[4096],scratch[4096];unsigned calls,fail,classes[5];bool failed;};
static void put(unsigned char*p,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(unsigned char)(v>>(8*i));}
static void image(unsigned char*b){
 memset(b,0,IMGSIZE);b[0x200]=0xeb;b[0x201]=0x66;
 put(b+0x1fe,0xaa55,2);put(b+0x202,0x53726448,4);put(b+0x206,0x20c,2);
 b[0x211]=1;b[0x234]=1;put(b+0x236,1,2);put(b+0x1f4,512,4);
 put(b+0x260,16384,4);put(b+0x230,4096,4);put(b+0x22c,LB_RAM_SIZE-1,4);put(b+0x238,4096,4);
 for(unsigned i=2560;i<IMGSIZE;i++)b[i]=(unsigned char)(i*13);
}
static bool span(const void*p,size_t n,const void*base,size_t size){
 uintptr_t a=(uintptr_t)p,b=(uintptr_t)base;return a>=b&&a-b<=size&&n<=size-(a-b);
}
static bool transfer(struct fixture*f,uint64_t gpa,const void*src,size_t n,bool zero){
 struct vbs s;if(f->failed||!vbs_init(&s,LB_RAM_SIZE,gpa,n))return false;
 uint64_t page;size_t off,k,done=0;
 while(vbs_next(&s,&page,&off,&k)){
  if(++f->calls==f->fail){f->failed=true;return false;}
  assert(off+k<=4096);
  if(zero)memset(f->ram+page*4096+off,0,k);else memcpy(f->ram+page*4096+off,(const unsigned char*)src+done,k);
  done+=k;
 }return done==n;
}
static bool z(void*c,uint64_t gpa,size_t n){return transfer(c,gpa,0,n,true);}
static bool w(void*c,enum cs_source kind,uint64_t gpa,const void*p,size_t n){
 struct fixture*f=c;if(f->failed)return false;
 switch(kind){
 case CS_IMAGE:if(!span(p,n,f->image,sizeof f->image))return false;break;
 case CS_INITRD:if(!span(p,n,f->rd,sizeof f->rd))return false;break;
 case CS_CMD:if(!span(p,n,f->cmd,sizeof f->cmd))return false;break;
 case CS_PARAMS:if(n!=4096||gpa!=0x90000)return false;memcpy(f->scratch,p,n);p=f->scratch;break;
 case CS_ENTRY:if(n!=8&&n!=2)return false;memcpy(f->scratch,p,n);p=f->scratch;break;
 default:return false;
 }
 f->classes[kind]++;return transfer(f,gpa,p,n,false);
}
static bool legacy_write(void*c,uint64_t gpa,const void*p,size_t n){
 unsigned char*ram=c;if(gpa>LB_RAM_SIZE||n>LB_RAM_SIZE-gpa)return false;memcpy(ram+gpa,p,n);return true;
}
static bool legacy_zero(void*c,uint64_t gpa,size_t n){
 unsigned char*ram=c;if(gpa>LB_RAM_SIZE||n>LB_RAM_SIZE-gpa)return false;memset(ram+gpa,0,n);return true;
}
int main(void){
 struct fixture*f=calloc(1,sizeof *f);assert(f);f->ram=malloc(LB_RAM_SIZE);unsigned char*old=malloc(LB_RAM_SIZE);assert(f->ram&&old);
 unsigned char original[IMGSIZE];image(original);memcpy(f->image,original,IMGSIZE);
 memset(f->rd,0xa6,sizeof f->rd);strcpy((char*)f->cmd,"console=ttyS0 rdinit=/bin/sh");
 /* Fixture owns private snapshots before BOTH inspections. Mutating original
  * demonstrates isolation of this copy, not atomic snapshot acquisition. */
 memset(original,0xff,sizeof original);
 struct cs_writer cw={f,w,z};struct lb_writer lw={old,legacy_write,legacy_zero};
 struct lb_plan a,b,inspect;struct lb_entry_state ea,eb;
 assert(cs_lb_inspect(f->image,IMGSIZE,sizeof f->rd,(char*)f->cmd,&inspect));
 memset(f->ram,0x55,LB_RAM_SIZE);memset(old,0x55,LB_RAM_SIZE);
 assert(cs_lb_load(f->image,IMGSIZE,f->rd,sizeof f->rd,(char*)f->cmd,&cw,&a));assert(cs_lb_entry_prepare(&a,&cw,&ea));
 assert(lb_load(f->image,IMGSIZE,f->rd,sizeof f->rd,(char*)f->cmd,&lw,&b));assert(lb_entry_prepare(&b,&lw,&eb));
 assert(!memcmp(f->ram,old,LB_RAM_SIZE));assert(!memcmp(&a,&b,sizeof a));assert(!memcmp(&inspect,&a,sizeof a));
 assert(ea.rip==eb.rip&&ea.rsi==eb.rsi&&ea.rsp==eb.rsp&&ea.cr0==eb.cr0&&ea.cr3==eb.cr3&&ea.cr4==eb.cr4&&ea.efer==eb.efer&&ea.gdt_base==eb.gdt_base&&ea.gdt_limit==eb.gdt_limit&&ea.cs==eb.cs&&ea.ds==eb.ds);
 for(unsigned i=0;i<5;i++)assert(f->classes[i]);
 unsigned total=f->calls;
 unsigned char foreign[8]={0};assert(!w(f,CS_IMAGE,0,foreign,8));assert(!w(f,CS_INITRD,0,f->cmd,8));
 assert(!w(f,CS_CMD,0,f->cmd+4095,2));assert(!w(f,CS_PARAMS,0,f->cmd,4096));assert(!w(f,CS_ENTRY,0,foreign,9));
 for(unsigned fail=1;fail<=total;fail++){
  f->calls=0;f->fail=fail;f->failed=false;
  bool ok=cs_lb_load(f->image,IMGSIZE,f->rd,sizeof f->rd,(char*)f->cmd,&cw,&a);
  if(ok)ok=cs_lb_entry_prepare(&a,&cw,&ea);
  assert(!ok&&f->failed&&f->calls==fail);assert(!z(f,0,1)&&f->calls==fail);
 }
 printf("producer source + bounded span parity and %u transfer failures PASS (host only)\n",total);
 free(old);free(f->ram);free(f);return 0;
}
