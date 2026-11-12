#include "nested_owner_ad.h"
#include "paging.h"
#include "vmx_memory.h"
#include <stddef.h>
struct nested_owner_ad_record {
    struct nested_owner_ad_record *next;
    struct nested_sparse_page allocation;
    uint64_t gpa,generation;
    struct nested_ept_result result;
    struct nested_ept_atomic_trace trace;
};
_Static_assert(sizeof(struct nested_owner_ad_record)<=4096,"one record per pool page");
static bool owner(struct nested_owner_ad *a) {
    if(!a||!a->bound||a->failed||!a->owner)return false;
    unsigned x,b,c,d;
    __asm__ volatile("cpuid":"=a"(x),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    struct nested_owner_sparse *s=a->owner;
    return s->pool.active&&!s->pool.fault&&s->pool.owner==(b>>24)&&
        s->initialized&&!s->destroyed&&!s->multiple&&s->tree.hardware_ad&&s->selected12==0x1005e;
}
static bool fail(struct nested_owner_ad *a) {
    if(a){
        a->failed=true;
        if(a->gate){
            ad_updater_gate_poison(a->gate);
            if(a->token.live)(void)ad_updater_gate_release(a->gate,&a->token,a->stamp,a->nonce);
        }
    }
    return false;
}
static bool guard(void *p,struct ad_entry_stamp stamp) {
    struct nested_owner_ad *a=p;
    return owner(a)&&ad_updater_gate_guard(a->gate,&a->token,stamp,a->nonce);
}
static bool backing(struct nested_owner_ad *a,uint64_t gpa,uint64_t *hpa,
                    uint64_t **view,uint8_t *permissions) {
    if(!owner(a)||!gpa||(gpa&4095)||gpa>=0x40000)return false;
    struct ept_fixture *f=a->owner->fixture;
    if(!f)return false;
    for(unsigned l=0;l<3;l++) {
        uint64_t *t=paging_phys_to_virt(f->tables[l]);
        if(!t||__atomic_load_n(&t[0],__ATOMIC_ACQUIRE)!=(f->tables[l+1]|7))return false;
    }
    uint64_t *pt=paging_phys_to_virt(f->tables[3]);
    if(!pt)return false;
    unsigned n=gpa/4096;
    uint64_t raw=__atomic_load_n(&pt[n],__ATOMIC_ACQUIRE);
    if((raw&~UINT64_C(7))!=(f->pages[n-1]|0x30))return false;
    *hpa=f->pages[n-1];*permissions=raw&7;
    *view=paging_phys_to_virt(*hpa);
    return *hpa&&*view&&!((uintptr_t)*view&4095)&&vmx_memory_region_wb(*hpa);
}
static bool table(void *p,uint64_t gpa,uint64_t *hpa,uint64_t **view) {
    uint8_t permissions;
    return gpa>=0x10000&&gpa<=0x13000&&
        backing(p,gpa,hpa,view,&permissions)&&permissions==1;
}
static bool leaf(void *p,uint64_t gpa,uint64_t *hpa,uint8_t *permissions) {
    uint64_t *view;
    return backing(p,gpa,hpa,&view,permissions);
}
bool nested_owner_ad_bind(struct nested_owner_ad *a,struct nested_owner_sparse *s) {
    if(!a||a->bound||a->failed||a->records||a->token.live||!a->authority||!s||!a->gate||
       !s->fixture||s->multiple||!s->initialized||s->destroyed||
       !s->tree.hardware_ad||s->selected12!=0x1005e||
       !a->stamp.domain_id||!a->stamp.incarnation||!a->stamp.context_id||
       !a->stamp.epoch||!a->stamp.lifetime)return false;
    a->owner=s;a->bound=true;
    return true;
}
static bool cached_valid(struct nested_owner_ad *a,struct nested_owner_ad_record *r) {
    const struct ad_entry_stamp *s=&r->trace.stamp,*t=&a->stamp;
    if(r->generation!=a->owner->tree.generation||s->domain_id!=t->domain_id||
       s->incarnation!=t->incarnation||s->context_id!=t->context_id||
       s->epoch!=t->epoch||s->lifetime!=t->lifetime||r->trace.count!=4)return false;
    for(unsigned i=0;i<4;i++) {
        struct nested_ept_atomic_entry *e=&r->trace.entry[i];
        uint64_t hpa,*page;
        if(!guard(a,a->stamp)||!table(a,e->entry_gpa&~UINT64_C(4095),&hpa,&page)||
           hpa!=e->table_hpa||&page[(e->entry_gpa&4095)/8]!=e->pointer)return false;
        uint64_t now=__atomic_load_n(e->pointer,__ATOMIC_ACQUIRE);
        if(!guard(a,a->stamp)||(now&~(e->leaf?UINT64_C(0x300):UINT64_C(0x100)))!=
           (e->raw&~(e->leaf?UINT64_C(0x300):UINT64_C(0x100))))return false;
    }
    uint64_t hpa;uint8_t permissions;
    if(!leaf(a,r->result.l1_gpa,&hpa,&permissions)||hpa!=r->result.hpa||
       (r->result.effective12&permissions)!=r->result.effective02)return false;
    return guard(a,a->stamp);
}
bool nested_owner_ad_fault(struct nested_owner_ad *a,uint64_t gpa,uint8_t need) {
    if(!owner(a)||a->reset_pending||!need||(need&~7))return fail(a);
    if(!ad_updater_gate_acquire(a->gate,a->stamp,&a->token,&a->nonce))return fail(a);
    uint64_t page=gpa&~UINT64_C(4095);
    struct nested_owner_ad_record *r=a->records;
    while(r&&r->gpa!=page)r=r->next;
    bool existing=r!=NULL;
    if((existing&&(a->promotions==UINT64_MAX||(page==0xf000&&a->target_promotions==UINT64_MAX)))||
       (!existing&&a->readwalks==UINT64_MAX))return fail(a);
    struct nested_ept_result cached;
    enum nested_sparse_lookup lookup=nested_ept_sparse_lookup(&a->owner->tree,page,1,&cached);
    if(existing) {
        bool tracked=false;
        if(lookup!=NESTED_SPARSE_HIT||!(need&2)||
           !nested_ept_sparse_query_tracking(&a->owner->tree,page,&tracked)||!tracked||
           !cached_valid(a,r)||cached.hpa!=r->result.hpa||
           cached.effective02!=r->result.effective02)return fail(a);
    }else{
        if(lookup!=NESTED_SPARSE_MISS)return fail(a);
        struct nested_sparse_page allocation;
        if(!nested_owner_pool_alloc(&a->owner->pool,&allocation))return fail(a);
        r=allocation.view;
        *r=(struct nested_owner_ad_record){.next=a->records,.allocation=allocation,
            .gpa=page,.generation=a->owner->tree.generation};
        a->records=r; /* ownership retained even if later walk fails */
    }
    const struct nested_ept_atomic_ops ops={.guard=guard,.table=table,.leaf=leaf};
    struct nested_ept_atomic_trace trace;
    struct nested_ept_atomic_result walk=nested_ept_atomic_walk(0x1005e,page,need,
        a->owner->pool.maxphys,&ops,a,a->stamp,&trace);
    if(walk.status!=NEA_DONE||walk.translation.status!=NESTED_EPT_OK)return fail(a);
    if(existing) {
        if(walk.translation.hpa!=r->result.hpa||walk.translation.l1_gpa!=r->result.l1_gpa||
           walk.translation.effective12!=r->result.effective12||
           walk.translation.effective02!=r->result.effective02||
           !nested_ept_sparse_promote_write(&a->owner->tree,page))return fail(a);
    }else{
        r->trace=trace;r->result=walk.translation;
        bool track=(walk.translation.effective02&2)&&!(need&2);
        if(nested_ept_sparse_fill_tracked(&a->owner->tree,page,&walk.translation,track)!=NESTED_SPARSE_OK)
            return fail(a);
    }
    if(!guard(a,a->stamp)||
       !ad_updater_gate_release(a->gate,&a->token,a->stamp,a->nonce))return fail(a);
    if(existing){a->promotions++;if(page==0xf000)a->target_promotions++;}
    else a->readwalks++;
    return true;
}
bool nested_owner_ad_release_records(struct nested_owner_ad *a,bool quiescent) {
    if(!a||!a->bound||a->failed||!quiescent||!a->owner||a->owner->tree.root||
       a->owner->tree.leaves||a->owner->pool.fault||a->token.live)return false;
    unsigned x,b,c,d;
    __asm__ volatile("cpuid":"=a"(x),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    if(!a->owner->pool.active||a->owner->pool.owner!=(b>>24))return false;
    while(a->records){
        struct nested_owner_ad_record *r=a->records;
        struct nested_sparse_page page=r->allocation;
        a->records=r->next;
        nested_owner_pool_free(&a->owner->pool,page);
        if(a->owner->pool.fault)return fail(a);
    }
    return true;
}


static bool stamp_equal(struct ad_entry_stamp a,struct ad_entry_stamp b) {
    return a.domain_id==b.domain_id&&a.incarnation==b.incarnation&&
        a.context_id==b.context_id&&a.epoch==b.epoch&&a.lifetime==b.lifetime;
}
bool nested_owner_ad_local_reset(struct nested_owner_ad *a,bool root_quiescent) {
    if(!owner(a)||!root_quiescent||a->reset_pending||a->token.live||
       !a->owner->tree.root||a->owner->tree.poisoned||
       a->owner->tree.generation==UINT64_MAX||
       !ad_updater_gate_drained(a->gate,a->stamp))return false;
    uint64_t generation=a->owner->tree.generation,eptp=a->owner->tree.eptp;
    struct nested_sparse_node *root=a->owner->tree.root;
    if(!nested_ept_sparse_reset(&a->owner->tree))return fail(a);
    if(a->owner->pool.fault||a->owner->tree.root!=root||a->owner->tree.eptp!=eptp||
       a->owner->tree.generation!=generation+1||a->owner->tree.leaves||
       a->owner->tree.fill_count)return fail(a);
    while(a->records) {
        struct nested_owner_ad_record *r=a->records;
        struct nested_sparse_page page=r->allocation;
        a->records=r->next;
        nested_owner_pool_free(&a->owner->pool,page);
        if(a->owner->pool.fault)return fail(a);
    }
    a->reset_generation=generation+1;
    a->reset_pending=true;
    return true;
}
bool nested_owner_ad_advance_stamp(struct nested_owner_ad *a,
    struct ad_entry_stamp old,struct ad_entry_stamp next) {
    if(!owner(a)||!a->reset_pending||a->token.live||a->records||
       !stamp_equal(a->stamp,old)||old.epoch==UINT64_MAX||
       !a->owner->tree.root||a->owner->tree.poisoned||a->owner->tree.leaves||
       a->owner->tree.fill_count||a->owner->tree.generation!=a->reset_generation||
       !ad_updater_gate_drained(a->gate,old))return false;
    struct ad_entry_stamp expected=old;expected.epoch++;
    if(!stamp_equal(expected,next))return false;
    a->stamp=next;a->reset_pending=false;
    return true;
}
