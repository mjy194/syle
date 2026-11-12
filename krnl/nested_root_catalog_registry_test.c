/* Reuse the exact production registry/pool fixture, not a model bridge. */
#define main old_registry_suite
#include "typed_root_registry_test.c"
#undef main
#include "nested_root_catalog.h"
static _Alignas(4096) uint64_t source_tables[4][512];
struct source_fixture {
 struct fixture *f;
 struct nested_root_catalog catalog;
 struct nrc_span spans[5];struct nrc_root roots[1];
 struct nrc_session session;uint64_t nonce;
 unsigned guards,fail_guard,authority_calls;
 bool deny,revoke_at_leaf;
};
static bool authoritative(void *context,const struct nrc_span *span,uint64_t gpa,struct nrc_page *out) {
 struct source_fixture *s=context;struct fixture *f=s->f;
 assert(!f->lock_depth&&!s->catalog.lock&&f->directory.operations==1);
 assert(f->bridge.refill_operation&&f->bridge.refill_slot&&f->bridge.refill_nonce);
 assert(!trl_close(&f->lease)&&!f->directory.closed);s->authority_calls++;
 if(s->deny&&span->role==NRC_DATA)return false;
 if(s->revoke_at_leaf&&span->role==NRC_DATA){s->revoke_at_leaf=false;s->roots[0].generation++;}
 *out=(struct nrc_page){span->object,span->lifetime,span->hpa+(gpa-span->gpa),
  span->table_view,span->permissions,6,true};return true;
}
static bool source_guard(void *context,const struct tr_registry_walk *w) {
 struct source_fixture *s=context;assert(!s->f->lock_depth&&s->f->directory.operations==1);
 s->guards++;
 return (!s->fail_guard||s->guards!=s->fail_guard)&&w->virtual_key==s->session.root.full_eptp&&
  w->backing_hpa==DATA&&w->type_epoch==1&&w->type==TRL_WB&&nrc_guard(&s->session,s->nonce);
}
static struct nested_ept_result translate(void *context,const struct tr_registry_walk *w,uint64_t gpa,uint8_t access) {
 struct source_fixture *s=context;assert(w->virtual_key==s->session.root.full_eptp);
 return nrc_walk(&s->session,s->nonce,gpa,access);
}
static void source_init(struct source_fixture *s,struct fixture *f) {
 *s=(struct source_fixture){.f=f};memset(source_tables,0,sizeof(source_tables));
 struct nrc_identity id={10,11,40,1};
 assert(nrc_init(&s->catalog,id,48,s->spans,5,s->roots,1,authoritative));
 for(unsigned i=0;i<4;i++) {
  struct nrc_span span={.identity=id,.object=i+1,.lifetime=i+1,.gpa=0x10000+i*4096,
   .hpa=0x10000000+i*4096,.bytes=4096,.table_view=source_tables[i],.permissions=1,
   .allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};
  assert(nrc_add_span(&s->catalog,span));
  if(i<3)source_tables[i][0]=(0x11000+i*4096)|7;
 }
 source_tables[3][15]=0xf073;
 struct nrc_span span={.identity=id,.object=5,.lifetime=88,.gpa=0xf000,.hpa=DATA,.bytes=4096,
  .permissions=3,.allowed_types=NRC_TYPE_WB,.role=NRC_DATA,.ignore_pat=true};
 assert(nrc_add_span(&s->catalog,span));
 assert(nrc_add_root(&s->catalog,(struct nrc_root){id,1,0,1,1,0x1001e,0x10000}));
 assert(nrc_publish(&s->catalog));
 assert(nrc_begin(&s->catalog,s->roots[0],s,&s->session,&s->nonce));
}
static void run_source(unsigned failure) {
 struct fixture f;init(&f,PAGES,SLOTS);synchronize(&f);
 assert(nested_ept_contexts_select(&f.registry,0x1001e)==NESTED_CONTEXT_OK);
 struct source_fixture source;source_init(&source,&f);
 const struct tr_registry_source ops={source_guard,translate};struct nested_ept_result r;
 size_t before=f.pool.live;
 if(failure==1)source.deny=true;
 if(failure==2)source.fail_guard=2;
 if(failure==3)source.fail_guard=3;
 if(failure==4)source.revoke_at_leaf=true;
 enum nested_sparse_fill status=typed_root_registry_miss_source(&f.bridge,f.registry.active,0xf123,3,&ops,&source,&r);
 if(!failure){
  assert(status==NESTED_SPARSE_OK&&r.status==NESTED_EPT_OK&&r.hpa==DATA);
  unsigned calls=source.authority_calls;
  assert(typed_root_registry_miss_source(&f.bridge,f.registry.active,0xf123,3,&ops,&source,&r)==NESTED_SPARSE_INVALID);
  assert(source.authority_calls==calls); /* warm cache is guarded, never retranslated */
 }else{
  assert(status!=NESTED_SPARSE_OK&&r.status==NESTED_EPT_L0_BACKING_DENIED);
  if(failure==3)assert(f.bridge.directory.faulted&&f.lease.poisoned&&f.pool.live>before);
  else assert(!f.bridge.directory.faulted&&f.pool.live==before);
 }
 assert(!f.directory.operations&&!f.bridge.refill_operation&&!f.bridge.refill_slot);
 if(failure==4){source.roots[0].generation--;assert(nrc_guard(&source.session,source.nonce));}
 assert(nrc_end(&source.session,source.nonce));
 if(failure!=3)assert(nested_ept_contexts_destroy(&f.registry));
}
int main(void){for(unsigned i=0;i<5;i++)run_source(i);puts("PASS N40A actual typed registry source guard, ownership, whole-miss permit and retained late failure");}
