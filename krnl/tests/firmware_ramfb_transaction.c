/* Reuse the existing real firmware-memory/page-walk test fixture. */
#define main fwcfg_legacy_fixture_main
#include "firmware_fwcfg_transaction.c"
#undef main
static void be32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(24-8*i));}
int main(void){
 struct heap*h=calloc(1,sizeof*h);struct firmware_memory*m=calloc(1,sizeof*m);assert(h&&m);
 assert(!posix_memalign((void**)&h->bytes,4096,(size_t)COUNT*4096));
 unsigned char code[65536]={0},vars[4096]={0};struct sg_memory_ops o={h,alloc,release,map,wb};assert(fwm_prepare(m,&o,code,sizeof code,vars,sizeof vars,36));
 struct fmm_memory f={.memory=m,.cr0=0x80010033,.cr3=0x1000,.cr4=0x2660,.efer=0x500,.cs_ar=0xa09b,.cs_selector=8};
 put(m,0x1000,0x2023);put(m,0x2000,0x3023);put(m,0x3000,0x4023);put(m,0x4000+9*8,0x9063);put(m,0x4000+10*8,0xa063);put(m,0x4000+16*8,0x10063);
 const uint8_t ins[]={0xef,0x90};assert(fwm_write_ram(m,0x10000,ins,2));
 for(unsigned test=0;test<5;test++){
  struct fwc_state s={0};assert(ffc_init(&s.stream,FWM_RAM_SIZE,1));
  struct fwc_exit e={.rip=0x10000,.qualification=0x5180003,.length=1,.flags=2,.dr7=0x400};
  uint64_t g[15]={0};g[0]=__builtin_bswap32(0x9000);g[3]=0x518;
  uint8_t d[16]={0},result[4]={0};be32(d,2);be32(d+4,4);be32(d+12,test==2?0x10000:test==3?0x4000:0xa000);
  assert(fwm_write_ram(m,0x9000,d,16));assert(fwm_write_ram(m,0xa000,result,4));struct commit c={.fail_vm=test==1};
  if(test==4){e.vectoring=0x80000020;}
  bool ok=fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr);
  assert(ok==(test==0));
  if(test<=1){assert(fwm_read_ram(m,0xa000,result,4)&&!memcmp(result,"QEMU",4));assert(fwm_read_ram(m,0x9000,result,4)&&!memcmp(result,"\0\0\0\0",4));}
  if(!test)assert(s.stream.cursor==4&&s.graphics.transfers==1&&c.rip==0x10001);
  else{assert(s.failed&&s.stream.cursor==0&&s.graphics.transfers==0);assert(!fwc_apply(&s,g,&f,&e,true,&c,ramwr,vmwr));}
 }
 assert(fwm_release(m)&&!h->live);free(h->bytes);free(h);free(m);
 puts("ramfb transaction: physical descriptor/owned target, completion, fatal post-DMA VMCS failure, code/page-table overwrite and vectoring rejection PASS");
}
