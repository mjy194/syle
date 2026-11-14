#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main legacy_session_main
#define lgm_release fixture_lgm_release
#include "../../krnl/tests/linux_guest_session.c"
#undef main
#pragma GCC diagnostic pop
#undef lgm_release
#include "../../krnl/linux_guest_role_binding.h"
static unsigned begins,ends,role_memory,role_host;
static bool begin_fail,end_fail,release_fail;
static uint64_t role(void *ctx,enum sg_memory_role kind){(void)ctx;(void)kind;return 0;}
bool lgm_release(struct linux_guest_memory *m){if(release_fail)return false;return fixture_lgm_release(m);}
bool lgm_prepare_roles(struct linux_guest_memory*m,const struct sg_memory_ops*o,sg_allocate_role_fn r,const void*k,size_t ks,const void*i,size_t is,const char*c){assert(r==role);role_memory++;return lgm_prepare(m,o,k,ks,i,is,c);}
bool lvh_open_roles(struct lvh_context*h,const struct lvh_ops*o,const struct linux_guest_memory*m,uint64_t rsp,uint64_t rip,sg_allocate_role_fn r){assert(r==role);role_host++;return lvh_open(h,o,m,rsp,rip);}
static bool begin(void *ctx){assert(ctx==&begins);begins++;return !begin_fail;}
static bool end(void *ctx){assert(ctx==&begins);assert(!s.host.active&&!s.enabled&&!s.memory.prepared);ends++;return !end_fail;}
/* This fixture exercises the no-barrier configuration only. */
bool lvh_close_with_barrier(struct lvh_context *h,bool (*hook)(void *),void *ctx){
 (void)h;(void)hook;(void)ctx;assert(!"barrier requires actual lvh test");return false;
}
int main(void){
 const char asset='x';struct lvh_ops h={.vmwrite=write,.invept=invalidate};struct lvs_ops v={0};
 struct lgs_platform p={.enable=enable,.disable=disable,.safe=safe,.read_field=rd,.dispatch=dispatch,.fatal=fatal};
 struct lgr_binding b={0};
 assert(!lgr_configure(&b,&s,&begins,role,begin,end));
 assert(lgs_configure(&s,&h,&v,&p,&asset,1,&asset,1,"",0x1000,0x1000));
 assert(lgs_prepare_with_path(&s,1,0));
 assert(lgr_configure(&b,&s,&begins,role,begin,end));
 assert(!lgr_configure(&b,&s,&begins,role,begin,end));
 assert(lgr_guest_backend.prepare(&b,2)&&!begins);
 begin_fail=true;assert(lgr_guest_backend.prepare(&b,1)&&b.active&&!role_memory);
 assert(lgr_guest_backend.prepare(&b,1)&&begins==1);
 assert(lgr_guest_backend.start(&b));
 assert(!lgr_guest_backend.release(&b)&&ends==1);begin_fail=false;
 assert(!lgr_guest_backend.prepare(&b,1)&&role_memory==1&&role_host==1);
 assert(!lgr_guest_backend.stop(&b)&&b.active&&s.host.active);
 unsigned oldfree=f.frees,oldend=ends;
 f.close_fail=1;assert(lgr_guest_backend.release(&b)&&f.frees==oldfree&&ends==oldend);
 assert(lgr_guest_backend.start(&b)&&lgr_guest_backend.prepare(&b,1));
 f.close_fail=0;release_fail=true;assert(lgr_guest_backend.release(&b)&&ends==oldend&&!s.host.active);
 release_fail=false;end_fail=true;assert(lgr_guest_backend.release(&b)&&b.base_released&&b.active);
 unsigned oldclose=f.closes;oldfree=f.frees;
 assert(lgr_guest_backend.release(&b)&&f.closes==oldclose&&f.frees==oldfree);
 assert(lgr_guest_backend.prepare(&b,1)&&lgr_guest_backend.start(&b));
 end_fail=false;assert(!lgr_guest_backend.release(&b)&&!b.active);
 assert(!lgr_guest_backend.prepare(&b,1));f.dispatch_fail=1;
 assert(lgr_guest_backend.start(&b)&&!b.ready);assert(lgr_guest_backend.start(&b));
 assert(!lgr_guest_backend.release(&b));f.dispatch_fail=0;
 f.prepare_fail=1;assert(lgr_guest_backend.prepare(&b,1)&&b.active);
 assert(!lgr_guest_backend.release(&b));f.prepare_fail=0;
 f.open_fail=1;assert(lgr_guest_backend.prepare(&b,1)&&s.host.active);
 assert(!lgr_guest_backend.release(&b));f.open_fail=0;
 assert(!lgr_guest_backend.prepare(&b,1));f.dirty=1;expect_fatal=true;oldend=ends;oldfree=f.frees;
 if(!setjmp(fatal_jump)){(void)lgr_guest_backend.start(&b);assert(!"dirty returned");}
 assert(b.active&&s.host.active&&ends==oldend&&f.frees==oldfree);
 puts("session role binding: actual session lifecycle, mocked VMX/RAM, retention/end ordering PASS");
}
