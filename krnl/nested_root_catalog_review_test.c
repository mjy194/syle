/* Independent N40A production-API boundaries. No privileged instructions. */
#include "nested_root_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define ROOTS 8u
#define PAGES 80u
#define DATA_GPA UINT64_C(0x900000)
#define DATA_HPA UINT64_C(0xa0000000)
#define INPUT_GPA UINT64_C(0x5000)
static _Alignas(4096) uint64_t tables[ROOTS][4][512];
struct fixture {
 struct nested_root_catalog c;
 struct nrc_span spans[ROOTS+2];
 struct nrc_root roots[ROOTS+1];
 struct nrc_identity id;
 unsigned calls,corrupt;
 uint8_t data_type;
 bool reject_data,close_in_callback;
};
static bool authority(void *opaque,const struct nrc_span *s,uint64_t gpa,struct nrc_page *p) {
 struct fixture *f=opaque; f->calls++;
 assert(f->c.lock==0); /* no metadata lock spans ownership calls */
 if(f->close_in_callback){f->close_in_callback=false;assert(nrc_close(&f->c,f->id));}
 if(f->reject_data&&s->role==NRC_DATA)return false;
 uint64_t offset=gpa-s->gpa;
 *p=(struct nrc_page){s->object,s->lifetime,s->hpa+offset,
   s->role==NRC_TABLE?(const uint64_t *)((uintptr_t)s->table_view+offset):NULL,
   s->permissions,s->role==NRC_TABLE?6:f->data_type,s->ignore_pat};
 switch(f->corrupt){
 case 1:p->object++;break;case 2:p->lifetime++;break;case 3:p->hpa+=4096;break;
 case 4:p->permissions^=2;break;case 5:p->memory_type=5;break;
 case 6:p->table_view=(void *)((uintptr_t)p->table_view+4096);break;
 case 7:p->ignore_pat=false;break;
 case 8:if(s->role==NRC_DATA)p->table_view=tables[0][0];break;
 case 9:f->roots[0].generation++;break;
 default:break;
 }
 return true;
}
static uint64_t rootg(unsigned i){return UINT64_C(0x80000)+(uint64_t)i*i*UINT64_C(0x20000);}
static void init(struct fixture *f) {
 memset(f,0,sizeof(*f));memset(tables,0,sizeof(tables));
 f->id=(struct nrc_identity){11,12,13,14};f->data_type=6;
 assert(nrc_init(&f->c,f->id,48,f->spans,ROOTS+2,f->roots,ROOTS+1,authority));
}
static struct nrc_span dataspan(struct fixture *f) {
 return (struct nrc_span){.identity=f->id,.object=200,.lifetime=201,
 .gpa=DATA_GPA,.hpa=DATA_HPA,.bytes=PAGES*4096,.permissions=3,
 .allowed_types=NRC_TYPE_WB|NRC_TYPE_UC,.role=NRC_DATA,.ignore_pat=true};
}
static void build(struct fixture *f,uint8_t data_permissions) {
 init(f);
 for(unsigned i=0;i<ROOTS;i++){
  uint64_t g=rootg(i);
  struct nrc_span s={.identity=f->id,.object=100+i,.lifetime=300+i,
   .gpa=g,.hpa=UINT64_C(0x80000000)+i*UINT64_C(0x20000),.bytes=16384,
   .table_view=tables[i][0],.permissions=1,.allowed_types=NRC_TYPE_WB,
   .role=NRC_TABLE,.ignore_pat=true};
  assert(nrc_add_span(&f->c,s));
  struct nrc_root r={.identity=f->id,.source=500+i,.owner=i%2,.generation=700+i,
   .lifetime=800+i,.full_eptp=g|0x1e,.root_gpa=g};
  assert(nrc_add_root(&f->c,r));
  for(unsigned j=0;j<3;j++)tables[i][j][0]=(g+(j+1)*4096)|7;
  tables[i][3][INPUT_GPA>>12]=(DATA_GPA+i*4096)|0x73;
 }
 struct nrc_span s=dataspan(f);s.permissions=data_permissions;
 assert(nrc_add_span(&f->c,s));assert(nrc_publish(&f->c));
}
static uint64_t begin(struct fixture *f,unsigned i,struct nrc_session *s) {
 struct nrc_root r;uint64_t n=0;
 assert(nrc_find(&f->c,f->id,rootg(i)|0x1e,&r));
 assert(nrc_begin(&f->c,r,f,s,&n));assert(n);return n;
}
static void positive(void) {
 struct fixture f;build(&f,3);
 for(unsigned i=0;i<ROOTS;i++){
  struct nrc_session s={0};uint64_t n=begin(&f,i,&s);
  struct nested_ept_result r=nrc_walk(&s,n,INPUT_GPA+37,3);
  assert(r.status==NESTED_EPT_OK&&r.hpa==DATA_HPA+i*4096+37);
  assert(r.l1_gpa==DATA_GPA+i*4096+37&&r.effective02==3);
  struct nrc_page p;
  assert(nrc_resolve(&s,n,DATA_GPA+(PAGES-1)*4096,1,&p));
  assert(p.hpa==DATA_HPA+(PAGES-1)*4096&&p.table_view==NULL);
  assert(!nrc_resolve(&s,n,DATA_GPA+PAGES*4096,1,&p));
  assert(!nrc_resolve(&s,n,DATA_GPA+1,1,&p));
  assert(nrc_end(&s,n));
 }
 assert(f.c.references==0);assert(nrc_close(&f.c,f.id));assert(nrc_drained(&f.c,f.id));
 puts("PASS review >3 irregular roots and >63 bounded backing pages");
}
static void registration(void) {
 struct fixture f;init(&f);struct nrc_span s=dataspan(&f),bad;
 #define BAD(expr) do{bad=s;expr;assert(!nrc_add_span(&f.c,bad));assert(f.c.span_count==0);}while(0)
 BAD(bad.identity.domain++);BAD(bad.identity.lifetime++);BAD(bad.object=0);BAD(bad.lifetime=0);
 BAD(bad.bytes=0);BAD(bad.bytes++);BAD(bad.gpa++);BAD(bad.hpa++);
 BAD(bad.gpa=(UINT64_C(1)<<48)-4096);BAD(bad.hpa=(UINT64_C(1)<<48)-4096);
 BAD(bad.gpa=UINT64_MAX-4095);BAD(bad.bytes=UINT64_MAX-4095);
 BAD(bad.permissions=2);BAD(bad.permissions=4);BAD(bad.permissions=0);BAD(bad.permissions=9);
 BAD(bad.allowed_types=1u<<4);BAD(bad.allowed_types=0);BAD(bad.ignore_pat=false);
 BAD(bad.table_view=tables[0][0]);BAD(bad.role=0);
 #undef BAD
 assert(nrc_add_span(&f.c,s));
 bad=s;bad.object++;bad.gpa+=0x100000;assert(!nrc_add_span(&f.c,bad)); /* HPA alias */
 bad=s;bad.object++;bad.hpa+=0x100000;assert(!nrc_add_span(&f.c,bad)); /* GPA alias */
 bad=s;bad.gpa+=0x100000;bad.hpa+=0x100000;assert(!nrc_add_span(&f.c,bad)); /* object reuse */
 bad=s;bad.object++;bad.gpa+=0x100000;bad.hpa+=0x100000;bad.role=NRC_TABLE;
 bad.table_view=tables[0][0];bad.permissions=1;bad.allowed_types=NRC_TYPE_WB;
 struct nrc_span t=bad;bad.permissions=3;assert(!nrc_add_span(&f.c,bad));
 bad=t;bad.allowed_types|=NRC_TYPE_UC;assert(!nrc_add_span(&f.c,bad));
 bad=t;bad.table_view=(void *)((uintptr_t)tables[0][0]+1);assert(!nrc_add_span(&f.c,bad));
 bad=t;bad.bytes=8192;bad.table_view=(void *)(UINTPTR_MAX&~(uintptr_t)4095);assert(!nrc_add_span(&f.c,bad));
 t.bytes=16384;assert(nrc_add_span(&f.c,t));
 struct nrc_root r={.identity=f.id,.source=1,.generation=1,.lifetime=1,.full_eptp=t.gpa|0x1e,.root_gpa=t.gpa},q;
 q=r;q.full_eptp|=0x40;assert(!nrc_add_root(&f.c,q));
 q=r;q.full_eptp^=6;assert(!nrc_add_root(&f.c,q));
 q=r;q.full_eptp|=0x80;assert(!nrc_add_root(&f.c,q));
 q=r;q.root_gpa+=4096;assert(!nrc_add_root(&f.c,q));
 q=r;q.full_eptp=DATA_GPA|0x1e;q.root_gpa=DATA_GPA;assert(!nrc_add_root(&f.c,q));
 q=r;q.generation=0;assert(!nrc_add_root(&f.c,q));
 assert(nrc_add_root(&f.c,r));assert(!nrc_add_root(&f.c,r));
 assert(nrc_publish(&f.c));assert(!nrc_add_span(&f.c,t));assert(!nrc_add_root(&f.c,r));
 assert(!nrc_find(&f.c,f.id,r.full_eptp^0x40,&q));
 assert(!nrc_find(&f.c,f.id,r.full_eptp^6,&q));
 struct nrc_identity foreign=f.id;foreign.catalog++;
 assert(!nrc_find(&f.c,foreign,r.full_eptp,&q));
 puts("PASS review range/alignment/overflow/alias/role/full-EPTP registration");
}
static void authority_failures(void) {
 for(unsigned c=1;c<=8;c++){
  struct fixture f;build(&f,3);struct nrc_session s={0};uint64_t n=begin(&f,0,&s);
  f.corrupt=c;struct nrc_page p={.object=UINT64_C(0xabcdef)},old=p;
  assert(!nrc_resolve(&s,n,c==8?DATA_GPA:rootg(0),1,&p));
  assert(!memcmp(&p,&old,sizeof(p)));
  assert(nrc_walk(&s,n,INPUT_GPA,1).status==NESTED_EPT_L0_BACKING_DENIED);
  assert(nrc_end(&s,n));
 }
 struct fixture f;build(&f,3);struct nrc_session s={0};uint64_t n=begin(&f,0,&s);
 uint64_t generation=f.roots[0].generation;f.roots[0].generation++;
 assert(!nrc_guard(&s,n));assert(nrc_walk(&s,n,INPUT_GPA,1).status!=NESTED_EPT_OK);
 assert(!nrc_end(&s,n));assert(s.live&&f.c.references==1);
 f.roots[0].generation=generation;
 f.corrupt=9;struct nrc_page p={.object=0xabcdef},saved=p;
 assert(!nrc_resolve(&s,n,rootg(0),1,&p));assert(!memcmp(&p,&saved,sizeof(p)));
 f.corrupt=0;f.roots[0].generation=generation;assert(nrc_end(&s,n));
 puts("PASS review authority identity/lifetime/HPA/view/type and stale source checks");
}
static void lifecycle(void) {
 struct fixture f;build(&f,3);assert(nrc_idle(&f.c,f.id));struct nrc_session s={0},other={0};uint64_t old=begin(&f,0,&s),n=0;assert(!nrc_idle(&f.c,f.id));
 struct nrc_session copied=s;assert(!nrc_guard(&copied,old));assert(!nrc_end(&copied,old));
 struct nrc_root r=s.root;assert(nrc_end(&s,old));assert(nrc_begin(&f.c,r,&f,&s,&n)&&n>old);
 assert(!nrc_guard(&s,old));assert(!nrc_end(&s,old));assert(s.live&&f.c.references==1);
 f.close_in_callback=true;assert(nrc_walk(&s,n,INPUT_GPA,1).status==NESTED_EPT_OK);
 assert(!nrc_begin(&f.c,r,&f,&other,&old));assert(!nrc_drained(&f.c,f.id));
 assert(nrc_guard(&s,n));assert(nrc_end(&s,n));assert(nrc_drained(&f.c,f.id));assert(!nrc_idle(&f.c,f.id));
 build(&f,3);s=(struct nrc_session){0};n=begin(&f,0,&s);
 f.c.lock=1;assert(!nrc_end(&s,n));assert(f.c.fault&&s.live&&f.c.references==1);
 f.c.lock=0;assert(!nrc_guard(&s,n));assert(nrc_end(&s,n));assert(!s.live&&!f.c.references);
 build(&f,3);s=(struct nrc_session){0};f.c.next_nonce=UINT64_MAX;
 assert(!nrc_begin(&f.c,f.roots[0],&f,&s,&n));assert(!s.live&&!f.c.references);
 puts("PASS review close/drain, same-storage nonce ABA, failed-release retention and exhaustion");
}
static void statuses(void) {
 struct fixture f;build(&f,3);struct nrc_session s={0};uint64_t n=begin(&f,0,&s);
 uint64_t *leaf=&tables[0][3][INPUT_GPA>>12],original=*leaf;
 *leaf=original&~UINT64_C(2);assert(nrc_walk(&s,n,INPUT_GPA,2).status==NESTED_EPT_L1_DENIED);
 f.reject_data=true;assert(nrc_walk(&s,n,INPUT_GPA,2).status==NESTED_EPT_L0_BACKING_DENIED);
 *leaf=UINT64_MAX&~UINT64_C(7);assert(nrc_walk(&s,n,INPUT_GPA,2).status==NESTED_EPT_L1_DENIED);
 *leaf=original|0x800;assert(nrc_walk(&s,n,INPUT_GPA,1).status==NESTED_EPT_UNSUPPORTED);
 *leaf=original;f.reject_data=false;
 uint64_t branch=tables[0][0][0];tables[0][0][0]=DATA_GPA|7;
 assert(nrc_walk(&s,n,INPUT_GPA,1).status==NESTED_EPT_L0_BACKING_DENIED);
 tables[0][0][0]=branch;
 for(uint8_t access=0;access<=8;access+=8)assert(nrc_walk(&s,n,INPUT_GPA,access).status==NESTED_EPT_UNSUPPORTED);
 assert(nrc_end(&s,n));
 build(&f,1);s=(struct nrc_session){0};n=begin(&f,0,&s);
 leaf=&tables[0][3][INPUT_GPA>>12];*leaf&=~UINT64_C(2);
 assert(nrc_walk(&s,n,INPUT_GPA,2).status==NESTED_EPT_L0_BACKING_DENIED); /* both deny */
 assert(nrc_end(&s,n));
 puts("PASS review complete-leaf L0 precedence, L1 nonpresent and unsupported statuses");
}
static void memory_types(void) {
 struct fixture f;build(&f,3);struct nrc_session s={0};uint64_t n=begin(&f,0,&s);
 uint64_t *leaf=&tables[0][3][INPUT_GPA>>12],original=*leaf;
 for(unsigned mt=0;mt<8;mt++)if(mt!=6){
  *leaf=(original&~UINT64_C(0x38))|((uint64_t)mt<<3);
  assert(nrc_walk(&s,n,INPUT_GPA,1).status!=NESTED_EPT_OK);
 }
 *leaf=original&~UINT64_C(0x40);assert(nrc_walk(&s,n,INPUT_GPA,1).status!=NESTED_EPT_OK);
 *leaf=(original&~UINT64_C(0x38));f.data_type=0;
 assert(nrc_walk(&s,n,INPUT_GPA,1).status==NESTED_EPT_OK);
 f.data_type=6;assert(nrc_walk(&s,n,INPUT_GPA,1).status!=NESTED_EPT_OK);
 assert(nrc_end(&s,n));
 puts("PASS review exact physical memory type and IPAT admission");
}
int main(int argc,char **argv) {
 const char *only=argc>1?argv[1]:"all";
 #define RUN(name) do{if(!strcmp(only,"all")||!strcmp(only,#name))name();}while(0)
 RUN(positive);RUN(registration);RUN(authority_failures);RUN(lifecycle);RUN(statuses);RUN(memory_types);
 puts("PASS independent N40A production catalog review");return 0;
}
