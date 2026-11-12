#include "nested_ept_sparse.h"
#include <stddef.h>
struct nested_sparse_node {
    struct nested_sparse_page self,table;
    struct nested_sparse_node *next;
};
struct nested_sparse_leaf {
    struct nested_sparse_page self;
    uint64_t gpa;
    struct nested_ept_result resolved;
    struct nested_sparse_leaf *next;
    bool committed,tracked;
    uint8_t installedperm;
};
_Static_assert(sizeof(struct nested_sparse_node)<=4096,"node metadata page");
_Static_assert(sizeof(struct nested_sparse_leaf)<=4096,"leaf metadata page");
static bool address_ok(const struct nested_ept_sparse *t,uint64_t p) {
    return !(p&4095) && p<(UINT64_C(1)<<t->maxphys);
}
static bool perm_ok(uint8_t p) {
    return p && !(p&~7u) && (!(p&2)||(p&1)) && (!(p&4)||(p&1));
}
static bool alloc_page(struct nested_ept_sparse *t,struct nested_sparse_page *p) {
    if(!t->ops.alloc(t->context,p)) return false;
    if(!address_ok(t,p->hpa)||!p->view||((uintptr_t)p->view&4095)) {
        t->ops.free(t->context,*p);
        return false;
    }
    return true;
}
static void zero_table(struct nested_sparse_node *n) {
    uint64_t *p=n->table.view;
    for(unsigned i=0;i<512;i++) p[i]=0;
}
static struct nested_sparse_node *new_node(struct nested_ept_sparse *t) {
    struct nested_sparse_page table,meta;
    if(!alloc_page(t,&table)) return NULL;
    if(!alloc_page(t,&meta)) {t->ops.free(t->context,table);return NULL;}
    struct nested_sparse_node *n=meta.view;
    *n=(struct nested_sparse_node){.self=meta,.table=table};
    zero_table(n);
    return n;
}
static void free_nodes(struct nested_ept_sparse *t,struct nested_sparse_node *n) {
    while(n) {
        struct nested_sparse_node *next=n->next;
        struct nested_sparse_page meta=n->self,table=n->table;
        t->ops.free(t->context,table);
        t->ops.free(t->context,meta);
        n=next;
    }
}
static void free_leaves(struct nested_ept_sparse *t) {
    struct nested_sparse_leaf *p=t->leaves;
    while(p) {
        struct nested_sparse_leaf *next=p->next;
        struct nested_sparse_page self=p->self;
        t->ops.free(t->context,self);
        p=next;
    }
    t->leaves=NULL;
}
static struct nested_sparse_node *find_node(struct nested_ept_sparse *t,uint64_t hpa) {
    for(struct nested_sparse_node *n=t->nodes;n;n=n->next)
        if(n->table.hpa==hpa) return n;
    return NULL;
}
bool nested_ept_sparse_init(struct nested_ept_sparse *t,
    const struct nested_sparse_ops *ops,void *context,unsigned maxphys) {
    return nested_ept_sparse_init_mode(t,ops,context,maxphys,false);
}
bool nested_ept_sparse_init_mode(struct nested_ept_sparse *t,
    const struct nested_sparse_ops *ops,void *context,unsigned maxphys,bool hardware_ad) {
    if(!t) return false;
    *t=(struct nested_ept_sparse){0};
    if(!ops||!ops->alloc||!ops->free||!ops->flush||maxphys<12||maxphys>48) return false;
    t->ops=*ops;t->context=context;t->maxphys=maxphys;t->hardware_ad=hardware_ad;
    t->root=new_node(t);
    if(!t->root) return false;
    t->nodes=t->root;t->eptp=t->root->table.hpa|(hardware_ad?0x5e:0x1e);
    /* A recycled root HPA can name an old cached EPT context. Retain all
     * ownership if initial invalidation fails, just as after publication. */
    if(!t->ops.flush(t->context,t->eptp)) {t->poisoned=true;return false;}
    t->generation=1;
    return true;
}
bool nested_ept_sparse_set_typed_admission(struct nested_ept_sparse *t,
    nested_sparse_typed_admit_fn admit,void *context) {
    if(!t||!t->root||t->poisoned||!admit||t->typed_admit||t->generation!=1||
       t->leaves||t->fill_count)return false;
    t->typed_admit=admit;t->typed_context=context;return true;
}
bool nested_ept_sparse_set_typed_admission_pat(struct nested_ept_sparse *t,nested_sparse_typed_admit_fn fn,void *ctx){
    if(!nested_ept_sparse_set_typed_admission(t,fn,ctx))return false;
    t->typed_allow_pat=true;return true;
}
bool nested_ept_sparse_set_typed_admission_wt(struct nested_ept_sparse *t,nested_sparse_typed_admit_fn fn,void *ctx){
    if(!nested_ept_sparse_set_typed_admission(t,fn,ctx))return false;
    t->typed_allow_wt=true;return true;
}
bool nested_ept_sparse_set_typed_admission_atomic_uc(struct nested_ept_sparse *t,nested_sparse_typed_admit_fn fn,void *ctx){
    if(!t||!t->hardware_ad||!nested_ept_sparse_set_typed_admission(t,fn,ctx))return false;
    t->typed_atomic_uc=true;return true;
}
static bool memory_admitted(const struct nested_ept_sparse *t,
    const struct nested_ept_result *r) {
    if(!r)return false;
    if(!t->typed_admit)
        return !r->memory_explicit || (r->leaf_type==6&&r->ignore_pat&&r->walk_type==6);
    if(!r->memory_explicit || r->walk_type!=6 || (!r->ignore_pat&&!t->typed_allow_pat) ||
       (r->leaf_type!=0&&r->leaf_type!=6&&!(r->leaf_type==4&&t->typed_allow_wt&&r->ignore_pat)) || (r->leaf_type!=6&&t->hardware_ad&&!(t->typed_atomic_uc&&r->leaf_type==0&&r->ignore_pat&&!(r->effective12&4)&&!(r->effective02&4))))return false;
    return t->typed_admit(t->typed_context,r);
}
static uint64_t memory_flags(const struct nested_ept_result *r) {
    return r->memory_explicit?((uint64_t)r->leaf_type<<3)|(r->ignore_pat?0x40:0):0x30;
}
enum nested_sparse_fill nested_ept_sparse_fill(struct nested_ept_sparse *t,
    uint64_t gpa,const struct nested_ept_result *r) {
    return nested_ept_sparse_fill_tracked(t,gpa,r,false);
}
enum nested_sparse_fill nested_ept_sparse_fill_tracked(struct nested_ept_sparse *t,
    uint64_t gpa,const struct nested_ept_result *r,bool trap_write) {
    if(!t||!t->root) return NESTED_SPARSE_INVALID;
    if(t->poisoned) return NESTED_SPARSE_POISONED;
    if(!r||!address_ok(t,gpa)||r->status!=NESTED_EPT_OK||!r->leaf_resolved||
       !address_ok(t,r->l1_gpa)||!address_ok(t,r->hpa)||
       !perm_ok(r->effective12)||!perm_ok(r->effective02)||
       (r->effective02&~r->effective12)||t->fill_count==UINT64_MAX||
       (trap_write&&(!t->hardware_ad||!(r->effective02&2))))
        return NESTED_SPARSE_INVALID;
    if(!memory_admitted(t,r))return NESTED_SPARSE_INVALID;
    for(struct nested_sparse_leaf *p=t->leaves;p;p=p->next)
        if(p->gpa==gpa) return NESTED_SPARSE_EXISTS;
    struct nested_sparse_node *current=t->root,*staged=NULL;
    uint64_t *publish_slot=NULL,publish_value=0;
    for(unsigned depth=0;depth<3;depth++) {
        unsigned shift=39-9*depth,index=(unsigned)((gpa>>shift)&511);
        uint64_t *slot=&((uint64_t *)current->table.view)[index];
        if(*slot) {
            if(((*slot&4095)&~(t->hardware_ad?UINT64_C(0x100):0))!=7) {free_nodes(t,staged);return NESTED_SPARSE_INVALID;}
            current=find_node(t,*slot&~UINT64_C(4095));
            if(!current) {free_nodes(t,staged);return NESTED_SPARSE_INVALID;}
        } else {
            struct nested_sparse_node *n=new_node(t);
            if(!n) {free_nodes(t,staged);return NESTED_SPARSE_OOM;}
            n->next=staged;staged=n;
            if(!publish_slot) {publish_slot=slot;publish_value=n->table.hpa|7;}
            else *slot=n->table.hpa|7; /* slot belongs only to offline nodes */
            current=n;
        }
    }
    uint64_t *leaf_slot=&((uint64_t *)current->table.view)[(gpa>>12)&511];
    if(*leaf_slot) {free_nodes(t,staged);return NESTED_SPARSE_INVALID;}
    struct nested_sparse_page meta;
    if(!alloc_page(t,&meta)) {free_nodes(t,staged);return NESTED_SPARSE_OOM;}
    struct nested_sparse_leaf *leaf=meta.view;
    *leaf=(struct nested_sparse_leaf){.self=meta,.gpa=gpa,.resolved=*r,.next=t->leaves,
        .tracked=trap_write,.installedperm=(uint8_t)(r->effective02&~(trap_write?2u:0u))};
    /* Everything has validated/allocated. Own every page BEFORE publication.
     * Cache stays uncommitted until the flush succeeds. */
    t->leaves=leaf;
    while(staged) {
        struct nested_sparse_node *next=staged->next;
        staged->next=t->nodes;t->nodes=staged;staged=next;
    }
    *leaf_slot=r->hpa|leaf->installedperm|memory_flags(r);
    if(publish_slot) *publish_slot=publish_value;
    if(!t->ops.flush(t->context,t->eptp)) {
        t->poisoned=true;return NESTED_SPARSE_FLUSH_FAILED;
    }
    leaf->committed=true;t->fill_count++;
    return NESTED_SPARSE_OK;
}
static bool cached_path_matches(const struct nested_ept_sparse *t,uint64_t gpa,
    const struct nested_sparse_leaf *cached,uint64_t **slot_out) {
    const struct nested_ept_result *r=&cached->resolved;
    if(!memory_admitted(t,r))return false;
    const struct nested_sparse_node *node=t->root;
    if(t->eptp!=(node->table.hpa|(t->hardware_ad?0x5e:0x1e))) return false;
    for(unsigned depth=0;depth<3;depth++) {
        uint64_t entry=((const uint64_t *)node->table.view)[(gpa>>(39-9*depth))&511];
        entry&=~(t->hardware_ad?UINT64_C(0x100):0);
        if((entry&4095)!=7) return false;
        const struct nested_sparse_node *next=NULL;
        /* Only allocator-owned metadata supplies host views. Never cast the
         * HPA from a hardware entry into a host pointer. */
        for(const struct nested_sparse_node *n=t->nodes;n;n=n->next)
            if(entry==(n->table.hpa|7)) {next=n;break;}
        if(!next) return false;
        node=next;
    }
    uint64_t *slot=&((uint64_t *)node->table.view)[(gpa>>12)&511];
    uint64_t leaf=*slot&~(t->hardware_ad?UINT64_C(0x300):0);
    if(leaf!=(r->hpa|cached->installedperm|memory_flags(r))) return false;
    if(slot_out) *slot_out=slot;
    return true;
}
enum nested_sparse_lookup nested_ept_sparse_lookup(const struct nested_ept_sparse *t,
    uint64_t gpa,uint8_t access,struct nested_ept_result *result) {
    if(!t||!t->root||!result||!access||(access&~7u)||gpa>=(UINT64_C(1)<<t->maxphys))
        return NESTED_SPARSE_BAD_LOOKUP;
    if(t->poisoned) return NESTED_SPARSE_LOOKUP_POISONED;
    uint64_t page=gpa&~UINT64_C(4095);
    for(const struct nested_sparse_leaf *p=t->leaves;p;p=p->next) if(p->gpa==page&&p->committed) {
        if(!cached_path_matches(t,gpa,p,NULL)) return NESTED_SPARSE_BAD_LOOKUP;
        struct nested_ept_result r=p->resolved;
        r.l1_gpa+=gpa&4095;r.hpa+=gpa&4095;
        if((r.effective12&access)!=access) r.status=NESTED_EPT_L1_DENIED;
        else if((r.effective02&access)!=access) r.status=NESTED_EPT_L0_BACKING_DENIED;
        else r.status=NESTED_EPT_OK;
        *result=r;return NESTED_SPARSE_HIT;
    }
    return NESTED_SPARSE_MISS;
}
bool nested_ept_sparse_query_tracking(const struct nested_ept_sparse *t,uint64_t gpa,
    bool *tracked) {
    if(!t||!t->root||t->poisoned||!tracked||gpa>=(UINT64_C(1)<<t->maxphys)) return false;
    for(const struct nested_sparse_leaf *p=t->leaves;p;p=p->next)
        if(p->gpa==(gpa&~UINT64_C(4095))&&p->committed) {
            if(!cached_path_matches(t,gpa,p,NULL)) return false;
            *tracked=p->tracked;
            return true;
        }
    return false;
}
bool nested_ept_sparse_promote_write(struct nested_ept_sparse *t,uint64_t gpa) {
    if(!t||!t->root||t->poisoned||!t->hardware_ad||gpa>=(UINT64_C(1)<<t->maxphys)) return false;
    for(struct nested_sparse_leaf *p=t->leaves;p;p=p->next)
        if(p->gpa==(gpa&~UINT64_C(4095))&&p->committed) {
            uint64_t *slot;
            if(!p->tracked||!(p->resolved.effective02&2)||
               !cached_path_matches(t,gpa,p,&slot)) return false;
            *slot=(*slot&~UINT64_C(7))|p->resolved.effective02;
            if(!t->ops.flush(t->context,t->eptp)) {t->poisoned=true;return false;}
            p->installedperm=p->resolved.effective02;
            p->tracked=false;
            return true;
        }
    return false;
}
bool nested_ept_sparse_reset_guarded(struct nested_ept_sparse *t,
    nested_sparse_retire_fn receipt,void *context) {
    if(!t||!t->root||t->poisoned||t->generation==UINT64_MAX) return false;
    zero_table(t->root);
    if(!t->ops.flush(t->context,t->eptp)) {t->poisoned=true;return false;}
    if(receipt&&!receipt(context,t)) {t->poisoned=true;return false;}
    free_leaves(t);
    /* Root may sit anywhere in the ownership list after staged publication. */
    struct nested_sparse_node *n=t->nodes;
    while(n) {
        struct nested_sparse_node *next=n->next;
        if(n!=t->root) {n->next=NULL;free_nodes(t,n);}
        n=next;
    }
    t->root->next=NULL;t->nodes=t->root;t->generation++;t->fill_count=0;
    return true;
}
bool nested_ept_sparse_reset(struct nested_ept_sparse *t) {
    return nested_ept_sparse_reset_guarded(t,NULL,NULL);
}
bool nested_ept_sparse_destroy_guarded(struct nested_ept_sparse *t,
    nested_sparse_retire_fn receipt,void *context) {
    if(!t) return false;
    if(!t->root) return true;
    if(!t->ops.flush(t->context,t->eptp)) {t->poisoned=true;return false;}
    if(receipt&&!receipt(context,t)) {t->poisoned=true;return false;}
    free_leaves(t);free_nodes(t,t->nodes);
    *t=(struct nested_ept_sparse){0};
    return true;
}

struct nested_ept_result nested_sparse_walk_access(struct nested_ept_result r,uint8_t access) {
    if(!access||(access&~7u)) {r.status=NESTED_EPT_MALFORMED_UNSUPPORTED;return r;}
    if(r.status!=NESTED_EPT_OK)return r;
    if((r.effective12&access)!=access)r.status=NESTED_EPT_L1_DENIED;
    else if((r.effective02&access)!=access)r.status=NESTED_EPT_L0_BACKING_DENIED;
    return r;
}

bool nested_ept_sparse_destroy(struct nested_ept_sparse *t) {
    return nested_ept_sparse_destroy_guarded(t,NULL,NULL);
}
