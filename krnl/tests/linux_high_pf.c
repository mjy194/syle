#include "../linux_high_pf.h"
#include <assert.h>
#include <stdio.h>
struct mock{unsigned n,fail;};
static bool put(void*p,uint32_t f,uint64_t v){struct mock*m=p;const uint32_t fields[]={0x4018,0x401a,0x4016};const uint64_t values[]={0,0,0x80000b0e};assert(m->n<3&&f==fields[m->n]&&v==values[m->n]);return ++m->n!=m->fail;}
int main(void){struct lpfg_state old={4,15,false,true};struct lhp_snapshot s={.permissions=true,.v={0,0x80000b0e,0,0xffff888000090000ULL,0xffffffff816cf50fULL,0x16f5000,0,0,0,0,0x80050033,0x20b0,0xd01,0xffff888000090000ULL,0xffffffff8174d760ULL,0x400,0xffffffff81603f38ULL,0x10,0xa09b,0,0x1c000,0xffffffff81611000ULL,0x7f,0x00af9b000000ffffULL,0xffffffff8174a000ULL,0xfff,0x816c8e000010f3deULL,0xffffffffULL,0,0xa5f3}};
 assert(lhp_accept(LHP_UNUSED,&old,&s));
 for(unsigned i=0;i<H_COUNT;i++){s.v[i]^=1;assert(!lhp_accept(LHP_UNUSED,&old,&s));s.v[i]^=1;}
 s.permissions=false;assert(!lhp_accept(LHP_UNUSED,&old,&s));s.permissions=true;
 assert(!lhp_accept(LHP_PENDING,&old,&s));assert(!lhp_accept(LHP_CONSUMED,&old,&s));
 old.pending=true;assert(!lhp_accept(LHP_UNUSED,&old,&s));old.pending=false;old.closed=false;assert(!lhp_accept(LHP_UNUSED,&old,&s));old.closed=true;
 enum lhp_state out=LHP_UNUSED;
 assert(lhp_consume(LHP_PENDING,28,0,0xb0e,&out)&&out==LHP_CONSUMED);
 assert(lhp_consume(LHP_PENDING,28,0,0,&out));assert(lhp_consume(LHP_PENDING,28,0,0x80000b0e,&out));
 assert(!lhp_consume(LHP_PENDING,0x80000000,0,0,&out));assert(out==LHP_CONSUMED);
 assert(!lhp_consume(LHP_PENDING,28,1,0,&out));assert(!lhp_consume(LHP_PENDING,28,0,0xb0f,&out));assert(!lhp_consume(LHP_UNUSED,28,0,0,&out));assert(!lhp_consume(LHP_CONSUMED,28,0,0,&out));
 for(unsigned fail=1;fail<=3;fail++){
  struct mock m={.fail=fail};enum lhp_state st=LHP_UNUSED;uint64_t cr2=0x123;uint32_t resume=0;
  assert(!lhp_commit(&st,&cr2,&resume,0xffff888000090000ULL,&m,put));assert(m.n==fail&&st==LHP_UNUSED&&cr2==0x123&&resume==0);
 }
 struct mock m={0};enum lhp_state st=LHP_UNUSED;uint64_t cr2=0;uint32_t resume=0;
 assert(lhp_commit(&st,&cr2,&resume,0xffff888000090000ULL,&m,put));assert(m.n==3&&st==LHP_PENDING&&cr2==0xffff888000090000ULL&&resume==1);
 m=(struct mock){0};assert(!lhp_commit(&st,&cr2,&resume,0xffff888000090000ULL,&m,put));assert(!m.n);
 puts("linux_high_pf: PASS");}
