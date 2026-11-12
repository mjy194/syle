#define main old_memory_review_main
#include "nested41c_memory.c"
#undef main
#include <sys/mman.h>
static unsigned guard_calls,fail_guard;
static bool is_uc;
static bool physical_uc(void *ctx,uint64_t hpa,uint64_t epoch){assert(ctx==&m);guard_calls++;return is_uc&&epoch==67&&hpa==m.owned[14]&&guard_calls!=fail_guard;}
static void fresh(void){base(&d);memset(&m,0,sizeof m);is_uc=false;guard_calls=fail_guard=0;assert(nested_atomic_memory_uc_stage(&m,&d,id,67));assert(d.atomic_catalog_retained&&!d.atomic_ad_prepared&&!m.catalog.published&&!m.prepared);}
static const uint64_t *root(unsigned n){return n==0?d.backing.tables:(n&1?d.root01[(n-1)/2]:d.root02[(n-1)/2]);}
static void detached(void){fresh();assert(nested_atomic_memory_uc_detach(&m));is_uc=true;denied_host=m.owned[14];}
static void inaccessible(void){detached();assert(!mprotect(ram[14],4096,PROT_NONE));assert(nested_atomic_memory_uc_install(&m,physical_uc,&m));assert(nested_atomic_memory_uc_installed(&m));assert(nested_atomic_memory_uc_publish(&m));assert(nested_atomic_memory_ready(&m));
 struct nrc_session s={0};uint64_t nonce;struct nrc_atomic_page p;assert(nrc_begin(&m.catalog,m.roots[0],&m,&s,&nonce));assert(nrc_resolve_atomic(&s,nonce,0xf000,1,&p)&&p.backing.memory_type==0&&!p.atomic_view);for(unsigned n=16;n<=27;n++)if((n>=16&&n<=20)||n>=24)assert(nrc_resolve_atomic(&s,nonce,n*4096,1,&p)&&p.backing.memory_type==6&&p.atomic_view&&p.pinned_writable);assert(nrc_end(&s,nonce));assert(!mprotect(ram[14],4096,PROT_READ|PROT_WRITE));puts("PASS independent N46B successful install/publish/resolve never dereferences protected target");}
static void all_words(void){unsigned count=0;fresh();assert(nested_atomic_memory_uc_detach(&m));is_uc=true;denied_host=m.owned[14];
 for(unsigned r=0;r<5;r++)for(unsigned depth=0;depth<4;depth++)for(unsigned slot=0;slot<512;slot++){uint64_t *page=paging_phys_to_virt(root(r)[depth]);uint64_t old=page[slot];page[slot]=old^UINT64_C(0x8000000000000000);assert(!nested_atomic_memory_uc_install(&m,physical_uc,&m)&&!m.uc_installed&&!m.catalog.published&&d.atomic_catalog_retained);assert(page[slot]==(old^UINT64_C(0x8000000000000000)));page[slot]=old;count++;}
 assert(count==10240);assert(nested_atomic_memory_uc_install(&m,physical_uc,&m));
 for(unsigned r=0;r<5;r++)for(unsigned depth=0;depth<4;depth++)for(unsigned slot=0;slot<512;slot++){uint64_t *page=paging_phys_to_virt(root(r)[depth]);uint64_t old=page[slot];page[slot]=old^UINT64_C(0x8000000000000000);assert(!nested_atomic_memory_uc_publish(&m)&&!m.catalog.published&&!m.prepared&&d.atomic_catalog_retained);assert(page[slot]==(old^UINT64_C(0x8000000000000000)));page[slot]=old;count++;}
 assert(count==20480);assert(nested_atomic_memory_uc_publish(&m));puts("PASS independent N46B all 20480 fixed-tree word corruptions fail before installation/publication");}
static void ownership(void){unsigned cases=0;
 for(unsigned bad=0;bad<NAM_OWNED_PAGES;bad++){if(bad==14)continue;detached();denied_host=m.owned[bad];assert(!nested_atomic_memory_uc_install(&m,physical_uc,&m)&&!m.uc_installed&&!m.catalog.published&&d.atomic_catalog_retained);cases++;}
 for(unsigned bad=0;bad<NAM_OWNED_PAGES;bad++){detached();m.owned[bad]^=4096;assert(!nested_atomic_memory_uc_install(&m,physical_uc,&m)&&!m.uc_installed&&!m.catalog.published&&d.atomic_catalog_retained);cases++;}
 for(unsigned bad=0;bad<9;bad++){detached();switch(bad){case 0:m.type_epoch++;break;case 1:m.catalog.type_epoch++;break;case 2:m.catalog.uc_gpa+=4096;break;case 3:m.catalog.uc_hpa+=4096;break;case 4:m.catalog.references++;break;case 5:m.catalog.closed=true;break;case 6:m.catalog.fault=1;break;case 7:m.large_2m=true;break;case 8:m.large_1g=true;break;}assert(!nested_atomic_memory_uc_install(&m,physical_uc,&m)&&!m.uc_installed&&!m.catalog.published&&d.atomic_catalog_retained);cases++;}
 assert(cases==174);puts("PASS independent N46B 174 ownership/policy/epoch failures retain unpublished memory");}
static void publication_failure(void){detached();assert(nested_atomic_memory_uc_install(&m,physical_uc,&m));fail_guard=guard_calls+2;assert(!nested_atomic_memory_uc_publish(&m));assert(m.catalog.published&&m.prepared&&d.atomic_ad_prepared&&d.atomic_catalog_retained);assert(!nested_atomic_memory_uc_publish(&m));is_uc=false;struct nrc_session s={0};uint64_t n;struct nrc_atomic_page p;assert(nrc_begin(&m.catalog,m.roots[0],&m,&s,&n));assert(!nrc_resolve_atomic(&s,n,0xf000,1,&p));assert(nrc_end(&s,n));puts("PASS independent N46B late publication failure is retained uncertain publication, never rollback");}
int main(void){setvbuf(stdout,NULL,_IONBF,0);inaccessible();all_words();ownership();publication_failure();return 0;}
