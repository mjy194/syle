#include "firmware_cmos_memory.h"
static bool same(const struct fwl_region*a,const struct fwl_region*b){
 return a->base==b->base&&a->size==b->size&&a->kind==b->kind&&a->read==b->read&&a->write==b->write&&a->execute==b->execute;
}
bool fcmi_init(struct fcmi_state*s,const struct fwl_plan*p){
 if(!s||s->ready||s->above16_low||s->above16_high||!p||p->ram.base||p->ram.size!=UINT64_C(0x8000000))return false;
 struct fwl_request request={p->ram.size,p->code.size,p->vars.size,32};struct fwl_plan canonical;
 if(!fwl_plan_build(&request,&canonical)||!same(&p->ram,&canonical.ram)||
 !same(&p->vars,&canonical.vars)||!same(&p->code,&canonical.code)||
 p->reset_linear!=canonical.reset_linear||p->reset_code_offset!=canonical.reset_code_offset)return false;
 uint64_t units=(p->ram.size-UINT64_C(0x1000000))>>16;
 *s=(struct fcmi_state){.above16_low=(uint8_t)units,.above16_high=(uint8_t)(units>>8),.ready=true};return true;
}
bool fcmi_read(const struct fcmi_state*s,uint8_t index,uint8_t*out){
 if(!s||!s->ready||!out||s->above16_low||s->above16_high!=7)return false;
 uint8_t value;
 switch(index){
 case 0x34:value=s->above16_low;break;
 case 0x35:value=s->above16_high;break;
 case 0x5b:case 0x5c:case 0x5d:value=0;break;
 default:return false;
 }
 *out=value;return true;
}
