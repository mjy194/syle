#include "linux_high_pf.h"
bool lhp_accept(enum lhp_state state,const struct lpfg_state*old,const struct lhp_snapshot*s){
 static const uint64_t exact[H_COUNT]={0,0x80000b0e,0,0xffff888000090000ULL,0xffffffff816cf50fULL,0x16f5000,0,0,0,0,0x80050033,0x20b0,0xd01,0xffff888000090000ULL,0xffffffff8174d760ULL,0x400,0xffffffff81603f38ULL,0x10,0xa09b,0,0x1c000,0xffffffff81611000ULL,0x7f,0x00af9b000000ffffULL,0xffffffff8174a000ULL,0xfff,0x816c8e000010f3deULL,0xffffffffULL,0,0xa5f3};
 if(!old||!s||state!=LHP_UNUSED||!s->permissions||old->count!=4||old->seen!=15||old->pending||!old->closed)return false;
 for(unsigned i=0;i<H_COUNT;i++)if(s->v[i]!=exact[i])return false;
 return true;
}
bool lhp_consume(enum lhp_state state,uint64_t reason,uint64_t vector,uint64_t event,enum lhp_state*out){
 if(!out||state!=LHP_PENDING||(reason&(1ULL<<31))||vector||
    (event!=0&&event!=0xb0e&&event!=0x80000b0e))return false;
 *out=LHP_CONSUMED;return true;
}

bool lhp_commit(enum lhp_state*state,uint64_t*cr2,uint32_t*resume,uint64_t fault,void*ctx,bool(*write)(void*,uint32_t,uint64_t)){
 if(!state||!cr2||!resume||!write||*state!=LHP_UNUSED||fault!=0xffff888000090000ULL)return false;
 if(!write(ctx,0x4018,0)||!write(ctx,0x401a,0)||!write(ctx,0x4016,0x80000b0e))return false;
 *cr2=fault;*state=LHP_PENDING;*resume=1;return true;
}
