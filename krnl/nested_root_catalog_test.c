#include "nested_root_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
static uint64_t tables[16][512] __attribute__((aligned(4096)));
struct fixture {
 struct nested_root_catalog catalog;
 struct nrc_span spans[80];struct nrc_root roots[8];
 struct nrc_identity identity;
 struct nrc_session session;uint64_t nonce;
 unsigned calls;
 bool denied,foreign,lifetime,permissions,type,view,close_on_callback,held;
};
static bool authority(void *context,const struct nrc_span *s,uint64_t gpa,struct nrc_page *out) {
 struct fixture *f=context;assert(!f->catalog.lock);f->calls++;
 if(f->denied)return false;
 uint64_t offset=gpa-s->gpa;
 *out=(struct nrc_page){s->object,s->lifetime,s->hpa+offset,
  s->table_view?(const uint64_t *)((uintptr_t)s->table_view+offset):NULL,s->permissions,6,true};
 if(f->foreign)out->hpa+=4096;
 if(f->lifetime)out->lifetime++;
 if(f->permissions)out->permissions^=2;
 if(f->type)out->memory_type=0;
 if(f->view)out->table_view=tables[15];
 if(f->close_on_callback){f->close_on_callback=false;assert(nrc_close(&f->catalog,f->identity));}
 return true;
}
static void setup(struct fixture *f) {
 memset(f,0,sizeof(*f));memset(tables,0,sizeof(tables));
 f->identity=(struct nrc_identity){1,2,3,4};
 assert(nrc_init(&f->catalog,f->identity,48,f->spans,80,f->roots,8,authority));
 for(unsigned k=0;k<4;k++) {
  uint64_t base=0x100000+0x17000*k;
  for(unsigned l=0;l<4;l++) {
   struct nrc_span s={.identity=f->identity,.object=1+k*4+l,.lifetime=0x100+k*4+l,
    .gpa=base+l*4096,.hpa=0x1000000+(k*4+l)*4096,.bytes=4096,.table_view=tables[k*4+l],
    .permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};
   assert(nrc_add_span(&f->catalog,s));
   if(l<3)tables[k*4+l][0]=(base+(l+1)*4096)|7;
   else tables[k*4+l][7]=(0x200000+k*0x3000)|0x73;
  }
  struct nrc_root r={f->identity,0x700+k,0,0x900+k,0xa00+k,base|0x1e,base};
  assert(nrc_add_root(&f->catalog,r));
 }
 for(unsigned n=0;n<64;n++) {
  struct nrc_span s={.identity=f->identity,.object=100+n,.lifetime=200+n,.gpa=0x200000+n*0x3000,
   .hpa=0x2000000+n*0x2000,.bytes=4096,.permissions=3,.allowed_types=NRC_TYPE_WB,.role=NRC_DATA,.ignore_pat=true};
  assert(nrc_add_span(&f->catalog,s));
 }
 assert(nrc_publish(&f->catalog));
}
static void begin(struct fixture *f,unsigned i){assert(nrc_begin(&f->catalog,f->roots[i],f,&f->session,&f->nonce));}
static void end(struct fixture *f){assert(nrc_end(&f->session,f->nonce));}
static void walks(void) {
 struct fixture f;setup(&f);
 assert(f.catalog.root_count==4&&f.catalog.span_count==80);
 for(unsigned i=0;i<4;i++) {
  begin(&f,i);
  struct nested_ept_result r=nrc_walk(&f.session,f.nonce,0x7123,3);
  assert(r.status==NESTED_EPT_OK&&r.l1_gpa==0x200000+i*0x3000+0x123&&r.hpa==0x2000000+i*0x2000+0x123);
  assert(r.memory_explicit&&r.leaf_type==6&&r.ignore_pat&&r.effective02==3);end(&f);
 }
 begin(&f,0);tables[3][7]&=~UINT64_C(2);
 assert(nrc_walk(&f.session,f.nonce,0x7000,2).status==NESTED_EPT_L1_DENIED);
 f.denied=true;assert(nrc_walk(&f.session,f.nonce,0x7000,2).status==NESTED_EPT_L0_BACKING_DENIED);f.denied=false;
 tables[3][7]=UINT64_MAX&~UINT64_C(7);f.calls=0;
 assert(nrc_walk(&f.session,f.nonce,0x7000,2).status==NESTED_EPT_L1_DENIED&&f.calls==4);
 tables[3][7]=0x200000|0x77|0x80;
 assert(nrc_walk(&f.session,f.nonce,0x7000,1).status==NESTED_EPT_UNSUPPORTED);
 tables[3][7]=0x200000|0x73;
 tables[2][0]=0x200000|7; /* an ordinary readable WB data page is NOT a table */
 assert(nrc_walk(&f.session,f.nonce,0x7000,1).status==NESTED_EPT_L0_BACKING_DENIED);
 end(&f);
}
static void checks(void) {
 struct fixture f;setup(&f);begin(&f,0);
 struct nrc_page out={.hpa=123};
 assert(!nrc_resolve(&f.session,f.nonce,UINT64_MAX-4095,1,&out)&&out.hpa==123);
 assert(!nrc_resolve(&f.session,f.nonce,0x200001,1,&out)&&out.hpa==123);
 assert(!nrc_resolve(&f.session,f.nonce,0x201000,1,&out)&&out.hpa==123);
 assert(!nrc_resolve(&f.session,f.nonce,0x200000,0,&out)&&out.hpa==123);
 bool *bad[]={&f.denied,&f.foreign,&f.lifetime,&f.permissions,&f.type,&f.view};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
  *bad[i]=true;assert(!nrc_resolve(&f.session,f.nonce,0x100000,1,&out));*bad[i]=false;
 }
 assert(nrc_resolve(&f.session,f.nonce,0x200000,4,&out)&&out.permissions==3&&!out.table_view);
 struct nrc_root stale=f.roots[0];stale.generation++;
 struct nrc_session other={0};uint64_t n=999;
 assert(!nrc_begin(&f.catalog,stale,&f,&other,&n)&&n==999&&!other.live);
 stale=f.roots[0];stale.identity.incarnation++;
 assert(!nrc_begin(&f.catalog,stale,&f,&other,&n));
 struct nrc_root result={.source=123};
 assert(!nrc_find(&f.catalog,f.identity,f.roots[0].full_eptp|0x40,&result)&&result.source==123);
 f.close_on_callback=true;
 assert(nrc_walk(&f.session,f.nonce,0x7000,1).status==NESTED_EPT_OK);
 assert(!nrc_begin(&f.catalog,f.roots[0],&f,&other,&n)&&!nrc_drained(&f.catalog,f.identity));
 assert(!nrc_find(&f.catalog,f.identity,f.roots[0].full_eptp,&result));
 end(&f);assert(nrc_drained(&f.catalog,f.identity));
}
static void lifetime(void) {
 struct fixture f;setup(&f);begin(&f,0);uint64_t old=f.nonce;end(&f);begin(&f,0);
 assert(f.nonce!=old&&!nrc_guard(&f.session,old)&&!nrc_end(&f.session,old));
 assert(f.session.live&&f.catalog.references==1);
 f.catalog.lock=1;
 assert(!nrc_end(&f.session,f.nonce)&&f.session.live&&f.catalog.references==1&&f.catalog.fault);
 f.catalog.lock=0;assert(!nrc_guard(&f.session,f.nonce));
 end(&f);assert(f.catalog.references==0&&!f.catalog.sessions);
 assert(!nrc_close(&f.catalog,f.identity)); /* poison is not release evidence */
 setup(&f);f.catalog.next_nonce=UINT64_MAX;uint64_t n=5;
 assert(!nrc_begin(&f.catalog,f.roots[0],&f,&f.session,&n)&&n==5);
}
static void registration(void) {
 struct fixture f={.identity={1,2,3,4}};
 assert(nrc_init(&f.catalog,f.identity,32,f.spans,80,f.roots,8,authority));
 struct nrc_span s={.identity=f.identity,.object=1,.lifetime=2,.gpa=0x1000,.hpa=0x2000,.bytes=8192,
  .table_view=tables[0],.permissions=1,.allowed_types=NRC_TYPE_WB,.role=NRC_TABLE,.ignore_pat=true};
 struct nrc_span bad=s;bad.hpa=UINT64_MAX-4095;assert(!nrc_add_span(&f.catalog,bad));
 bad=s;bad.gpa=UINT64_C(1)<<32;assert(!nrc_add_span(&f.catalog,bad));
 bad=s;bad.gpa=1;assert(!nrc_add_span(&f.catalog,bad));
 bad=s;bad.bytes=UINT64_MAX-4095;assert(!nrc_add_span(&f.catalog,bad));
 bad=s;bad.permissions=3;assert(!nrc_add_span(&f.catalog,bad));
 bad=s;bad.allowed_types|=NRC_TYPE_UC;assert(!nrc_add_span(&f.catalog,bad));
 bad=s;bad.role=NRC_DATA;assert(!nrc_add_span(&f.catalog,bad));
 bad=s;bad.ignore_pat=false;assert(!nrc_add_span(&f.catalog,bad));
 assert(!f.catalog.span_count&&nrc_add_span(&f.catalog,s));
 bad=s;bad.object=2;bad.gpa=0x8000;assert(!nrc_add_span(&f.catalog,bad)); /* HPA alias */
 bad=s;bad.object=2;bad.hpa=0x8000;assert(!nrc_add_span(&f.catalog,bad)); /* GPA overlap */
 struct nrc_root r={f.identity,1,0,1,1,0x101e,0x1000},rb=r;
 rb.full_eptp|=0x40;assert(!nrc_add_root(&f.catalog,rb));
 rb=r;rb.root_gpa+=4096;assert(!nrc_add_root(&f.catalog,rb));
 rb=r;rb.full_eptp=0x1018;assert(!nrc_add_root(&f.catalog,rb));
 rb=r;rb.full_eptp|=UINT64_C(1)<<32;assert(!nrc_add_root(&f.catalog,rb));
 assert(nrc_add_root(&f.catalog,r)&&!nrc_add_root(&f.catalog,r));
 assert(nrc_publish(&f.catalog)&&!nrc_add_span(&f.catalog,s)&&!nrc_add_root(&f.catalog,r));
}
int main(void){walks();checks();lifetime();registration();puts("PASS N40A catalog ownership/ranges/roles/identity/whole-walk lifetime and fault classes");}
