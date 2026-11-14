#include "linux_guest_role_binding.h"
bool lgr_configure(struct lgr_binding *b,struct lgs_session *s,void *ctx,
 sg_allocate_role_fn role,bool (*begin)(void *),bool (*end)(void *)) {
 if(!b||!s||!role||!begin||!end||b->configured||b->active||!s->configured||
 s->prepared||s->enabled||s->failed||s->host.active||s->host.vmxon_page||
 s->host.vmcs_page||s->memory.prepared||s->state.ready)return false;
 *b=(struct lgr_binding){.session=s,.context=ctx,.allocate_role=role,
 .begin=begin,.end=end,.configured=true};return true;
}
bool lgr_configure_barrier(struct lgr_binding *b,struct lgs_session *s,void *ctx,
 sg_allocate_role_fn role,bool (*begin)(void *),bool (*end)(void *),
 bool (*barrier)(void *),void *barrier_ctx){
 if(!barrier||!lgr_configure(b,s,ctx,role,begin,end))return false;
 b->close_barrier=barrier;b->barrier_context=barrier_ctx;return true;
}
static bool barrier(void *ctx){
 struct lgr_binding *b=ctx;
 if(!b->close_inflight||!b->active||b->base_released||b->ready||
 b->session->host.active||b->session->host.ready||!b->close_barrier)return false;
 return b->close_barrier(b->barrier_context);
}
static bool close_host(struct lvh_context *h,void *ctx){
 struct lgr_binding *b=ctx;
 if(h!=&b->session->host||b->close_inflight||!b->active)return false;
 b->close_inflight=true;
 bool ok=lvh_close_with_barrier(h,barrier,b);
 b->close_inflight=false;return ok;
}
static int prepare(void *ctx,uint64_t profile) {
 struct lgr_binding *b=ctx;
 if(!b||!b->configured||b->active||profile!=SYLE_GUEST_BOOT_PROFILE)return 1;
 b->active=true;b->ready=false;b->base_released=false;
 if(!b->begin(b->context))return 1;
 const struct lgs_prepare_path path={b->allocate_role,lgm_prepare_roles,lvh_open_roles};
 if(lgs_prepare_with_path(b->session,profile,&path))return 1;
 b->ready=true;return 0;
}
static int start(void *ctx) {
 struct lgr_binding *b=ctx;
 if(!b||!b->configured||!b->active||!b->ready)return 1;
 int r=lgs_guest_backend.start(b->session);if(r)b->ready=false;return r;
}
static int stop(void *ctx) {
 struct lgr_binding *b=ctx;
 if(!b||!b->configured||!b->active)return 1;
 /* Controller REAP calls stop again after a failed end. Base is already
  * fully released: idempotent success permits retrying retained end. */
 if(b->base_released)return 0;
 return lgs_guest_backend.stop(b->session);
}
static int release(void *ctx) {
 struct lgr_binding *b=ctx;
 if(!b||!b->configured)return 1;
 if(!b->active)return 0;
 b->ready=false;
 if(!b->base_released){
  if(b->close_barrier){
   const struct lgs_release_path path={close_host,b};
   if(lgs_release_with_path(b->session,&path))return 1;
  }else if(lgs_guest_backend.release(b->session))return 1;
  b->base_released=true;
 }
 if(!b->end(b->context))return 1;
 b->active=false;return 0;
}
const struct guest_backend lgr_guest_backend={.prepare=prepare,.start=start,.stop=stop,.release=release};
