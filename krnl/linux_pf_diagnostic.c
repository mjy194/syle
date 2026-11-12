#include "linux_pf_diagnostic.h"
#include "linux_guest_session.h"
static bool owned64(void*p,uint64_t a,uint64_t*out){
 struct lgs_session*s=p;if(!out||a>LGM_RAM_SIZE-8||!s->memory.prepared||!s->memory.owner.map)return false;
 uint64_t v=0;
 for(unsigned i=0;i<8;i++){
  uint64_t b=a+i,pa=s->memory.pages[b>>12];if(!pa||(pa&4095)||(pa>>52))return false;
  const unsigned char*m=s->memory.owner.map(s->memory.owner.context,pa);if(!m)return false;
  v|=(uint64_t)m[b&4095]<<(8*i);
 }*out=v;return true;
}
static bool virt64(struct lgs_session*s,uint64_t root,uint64_t va,uint64_t*out){
 uint64_t v=0;if(va>UINT64_MAX-7)return false;
 for(unsigned i=0;i<8;i++){
  struct lpt_trace t;lpt_walk(root,va+i,LGM_RAM_SIZE,s,owned64,&t);
  uint64_t b;if(t.status!=1||!owned64(s,t.gpa&~UINT64_C(7),&b))return false;
  v|=((b>>((t.gpa&7)*8))&255)<<(i*8);
 }*out=v;return true;
}
bool lgpd_collect(struct lgs_session*s,struct lgpd_snapshot*out){
 if(!s||!out||!s->memory.prepared||!s->host.active||!s->host.ready||s->state.host_dirty||!s->platform.read_field)return false;
 *out=(struct lgpd_snapshot){0};
 const uint32_t fields[LGPD_FIELDS]={0x4402,0x4404,0x4406,0x6400,0x681e,0x6800,0x6802,0x6804,0x2806,0x6818,0x4812,0x6816,0x4810,0x681c,0x0802,0x4816,0x6808,0x4802,0x0804,0x680a,0x4804,0x4818,0x080e,0x6814,0x480e,0x4822,0x6820,0x6822};
 for(unsigned i=0;i<LGPD_FIELDS;i++)if(s->platform.read_field(s->platform.ctx,fields[i],&out->value[i]))out->valid|=UINT64_C(1)<<i;
 out->r12=s->native.gpr[11];
 if(out->valid!=((UINT64_C(1)<<LGPD_FIELDS)-1)||out->value[0]||out->value[1]!=0x80000b0e)return true;
 uint64_t*r=out->value,cr3=r[6];out->va[0]=r[3];out->va[1]=r[4];
 for(unsigned i=0;i<2;i++)lpt_walk(cr3,out->va[i],LGM_RAM_SIZE,s,owned64,&out->trace[i]);
 if(r[10]>=239&&r[9]<=UINT64_MAX-239){
  out->va[2]=r[9]+224;lpt_walk(cr3,out->va[2],LGM_RAM_SIZE,s,owned64,&out->trace[2]);
  if(virt64(s,cr3,out->va[2],&out->gate[0])&&virt64(s,cr3,out->va[2]+8,&out->gate[1])){
   out->data_valid|=1;uint64_t g=out->gate[0];out->va[5]=(g&65535)|((g>>32)&UINT64_C(0xffff0000))|((out->gate[1]&UINT32_MAX)<<32);
   lpt_walk(cr3,out->va[5],LGM_RAM_SIZE,s,owned64,&out->trace[5]);
   if(out->va[5]<=UINT64_MAX-15&&virt64(s,cr3,out->va[5],&out->handler_bytes[0])&&virt64(s,cr3,out->va[5]+8,&out->handler_bytes[1]))out->data_valid|=8;
  }
 }
 uint64_t off=r[14]&~UINT64_C(7);
 if(!(r[14]&4)&&r[14]<=65535&&off<=r[12]&&r[12]-off>=7&&r[11]<=UINT64_MAX-off){
  out->va[3]=r[11]+off;lpt_walk(cr3,out->va[3],LGM_RAM_SIZE,s,owned64,&out->trace[3]);
  if(virt64(s,cr3,out->va[3],&out->csdesc))out->data_valid|=2;
 }
 if(r[13]>=32){out->va[4]=r[13]-32;lpt_walk(cr3,out->va[4],LGM_RAM_SIZE,s,owned64,&out->trace[4]);}
 if(r[4]<=UINT64_MAX-15&&virt64(s,cr3,r[4],&out->bytes[0])&&virt64(s,cr3,r[4]+8,&out->bytes[1]))out->data_valid|=4;

 /* Additional structural observation only; cached TR is authoritative for
  * TSS base, not the memory descriptor. No permission claim or guest writes. */
 if(out->data_valid&1){
  uint64_t sel=(out->gate[0]>>16)&65535,off=sel&~UINT64_C(7);
  if(!(sel&4)&&off&&off<=r[12]&&r[12]-off>=7&&r[11]<=UINT64_MAX-off){
   out->va[6]=r[11]+off;lpt_walk(cr3,out->va[6],LGM_RAM_SIZE,s,owned64,&out->trace[6]);
   if(virt64(s,cr3,out->va[6],&out->target_csdesc))out->data_valid|=16;
  }
 }
 uint64_t troff=r[22]&~UINT64_C(7);
 if(r[22]<=65535&&!(r[22]&4)&&troff&&troff<=r[12]&&r[12]-troff>=15&&r[11]<=UINT64_MAX-troff-15){
  out->va[7]=r[11]+troff;lpt_walk(cr3,out->va[7],LGM_RAM_SIZE,s,owned64,&out->trace[7]);
  if(virt64(s,cr3,out->va[7],&out->trdesc[0])&&virt64(s,cr3,out->va[7]+8,&out->trdesc[1]))out->data_valid|=32;
 }
 if(r[24]>=11&&r[23]<=UINT64_MAX-11){
  out->va[8]=r[23]+4;lpt_walk(cr3,out->va[8],LGM_RAM_SIZE,s,owned64,&out->trace[8]);
  if(virt64(s,cr3,out->va[8],&out->tss_rsp0))out->data_valid|=64;
 }
 if(out->data_valid&1){
  unsigned ist=(out->gate[0]>>32)&7;uint64_t off=ist?36+8*(ist-1):4;
  if(r[24]>=off+7&&r[23]<=UINT64_MAX-off-7){
   out->selected_slot=out->va[9]=r[23]+off;
   lpt_walk(cr3,out->va[9],LGM_RAM_SIZE,s,owned64,&out->trace[9]);
   if(virt64(s,cr3,out->va[9],&out->selected_stack)){
    out->data_valid|=128;uint64_t top=out->selected_stack&~UINT64_C(15);
    if(top>=48){out->va[10]=top-48;out->va[11]=top-1;
     for(unsigned i=10;i<12;i++)lpt_walk(cr3,out->va[i],LGM_RAM_SIZE,s,owned64,&out->trace[i]);
    }
   }
  }
 }
 return true;
}
