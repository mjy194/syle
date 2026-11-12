#include "large_maintenance_helpers.inc"
static void populate_large(struct mr_fixture *r){for(unsigned c=0;c<2;c++){
 struct review_owner *o=&r->f->owners[c];select_root(o,c);fault(o,1,TR_ATOMIC_FILLED);
 assert(!(physical_leaf(o,REVIEW_GPA)&2));fault(o,2,TR_ATOMIC_PROMOTED);assert(physical_leaf(o,REVIEW_GPA)&2);
 struct nested_ept_result out;assert(tr_atomic_fault(&o->atomic,o->registry.active,0x10000,1,&out)==TR_ATOMIC_FILLED);assert(!(physical_leaf(o,0x10000)&2));
 assert(tr_atomic_fault(&o->atomic,o->registry.active,0x10000,2,&out)==TR_ATOMIC_PROMOTED);
 struct nested_ept_context_item *pin=nested_ept_contexts_pin_active(&o->registry);assert(pin);assert(tr_atomic_entry(&o->atomic,pin,pin->tree.eptp));r->reference[c]=pin;r->physical_eptp[c]=pin->tree.eptp;}
 assert((review_tables[2][0]&0x300)==0x300);}
static void cycle(void){struct mr_fixture *r=mr_new();populate_large(r);struct nrc_root roots[3];memcpy(roots,r->f->roots,sizeof roots);
 uint64_t eptps[2]={r->physical_eptp[0],r->physical_eptp[1]};
 for(unsigned round=0;round<2;round++){
  __atomic_fetch_or(&review_tables[3][15],0x300,__ATOMIC_ACQ_REL);__atomic_fetch_or(&review_tables[8][15],0x300,__ATOMIC_ACQ_REL);
  mr_close_drain(r);mr_revoke(r);mr_clear(r);assert(r->m.page_count==9&&r->m.state==TAM_CLEARED);
  assert(!(review_tables[2][0]&0x300)&&!(review_tables[3][15]&0x300)&&!(review_tables[8][15]&0x300));
  assert(review_tables[2][0]==0xf3&&review_tables[2][1]==0x4007&&!memcmp(roots,r->f->roots,sizeof roots));
  mr_reset(r);mr_prepare(r);assert(tam_finish(&r->m,r->ticket));r->f->stamp=r->m.next_stamp;
  for(unsigned c=0;c<2;c++){struct review_owner *o=&r->f->owners[c];physical_cpu=c;assert(r->physical_eptp[c]==eptps[c]);assert(tr_atomic_entry(&o->atomic,r->reference[c],r->physical_eptp[c]));fault(o,1,TR_ATOMIC_FILLED);assert(!(physical_leaf(o,REVIEW_GPA)&2));}
  assert((review_tables[2][0]&0x300)==0x100);
  for(unsigned c=0;c<2;c++){struct review_owner *o=&r->f->owners[c];fault(o,2,TR_ATOMIC_PROMOTED);struct nested_ept_result out;assert(tr_atomic_fault(&o->atomic,o->registry.active,0x10000,1,&out)==TR_ATOMIC_FILLED);assert(!(physical_leaf(o,0x10000)&2));assert(tr_atomic_fault(&o->atomic,o->registry.active,0x10000,2,&out)==TR_ATOMIC_PROMOTED);}
  assert((review_tables[2][0]&0x300)==0x300);
 }
 mr_clean(r);puts("PASS N44 independent two maintenance rounds clear huge D, preserve 4KiB paths, sparse holes, identities and private first writes");}
static void invalid_graph(void){const uint64_t corrupt[]={0x1000,0x100000,UINT64_C(1)<<48,0x400};for(unsigned i=0;i<sizeof corrupt/sizeof *corrupt;i++){
 struct mr_fixture *r=mr_new();populate_large(r);mr_close_drain(r);mr_revoke(r);__atomic_fetch_or(&review_tables[2][0],corrupt[i],__ATOMIC_ACQ_REL);uint64_t before[9][512];memcpy(before,review_tables,sizeof before);physical_cpu=0;assert(!tam_clear(&r->m,r->ticket)&&r->m.state==TAM_POISONED);assert(!memcmp(before,review_tables,sizeof before)&&!r->cas_calls);
 /* Poisoned roots/backing intentionally retained by contract, no fake teardown. */
 }puts("PASS N44 independent malformed huge graph rejected before any maintenance source mutation");}
static void clear_failure(void){struct mr_fixture *r=mr_new();populate_large(r);mr_close_drain(r);mr_revoke(r);r->fail_cas=3;physical_cpu=0;assert(!tam_clear(&r->m,r->ticket)&&r->m.state==TAM_POISONED);assert((review_tables[2][0]&0x300)==0x300&&!r->opens);assert(r->m.changed);puts("PASS N44 independent partial clear failure retains source pins and stops before huge D clear");}
static void owner_gate(void){struct mr_fixture *r=mr_new();populate_large(r);mr_close_drain(r);physical_cpu=0;assert(tam_revoke(&r->m,0,r->ticket));assert(!tam_clear(&r->m,r->ticket));assert(r->m.state==TAM_DRAINED&&!r->cas_calls&&(review_tables[2][0]&0x300)==0x300);physical_cpu=1;assert(tam_revoke(&r->m,1,r->ticket));mr_clear(r);mr_reset(r);mr_prepare(r);assert(tam_finish(&r->m,r->ticket));mr_clean(r);puts("PASS N44 independent missing-owner revoke rejects unchanged without premature poison or clear");}
static void late_dirty(void){struct mr_fixture *r=mr_new();populate_large(r);mr_close_drain(r);mr_revoke(r);mr_clear(r);mr_reset(r);mr_prepare(r);__atomic_fetch_or(&review_tables[2][0],0x200,__ATOMIC_ACQ_REL);assert(!tam_finish(&r->m,r->ticket)&&r->m.state==TAM_POISONED&&!r->opens&&r->entry_closed);puts("PASS N44 independent late huge-D reappearance blocks reopen");}
int main(int argc,char **argv){const char *s=argc>1?argv[1]:"all";if(!strcmp(s,"all")||!strcmp(s,"cycle"))cycle();if(!strcmp(s,"all")||!strcmp(s,"invalid_graph"))invalid_graph();if(!strcmp(s,"all")||!strcmp(s,"failures"))clear_failure();if(!strcmp(s,"all")||!strcmp(s,"owner_gate"))owner_gate();if(!strcmp(s,"all")||!strcmp(s,"late_dirty"))late_dirty();return 0;}
