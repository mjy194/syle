#include "nested_ept_contexts.h"
#include <stddef.h>
#define ROOT_MASK UINT64_C(0x000ffffffffff000)
_Static_assert(sizeof(struct nested_ept_context_item)<=4096,"context metadata fits one page");
static bool valid_key(const struct nested_ept_contexts *r,uint64_t key) {
    uint64_t mode=key&4095;
    return (r->mixed_ad?(mode==0x1e||mode==0x5e):mode==(r->hardware_ad?0x5e:0x1e)) &&
           (key>>r->maxphys)==0;
}
static bool healthy(struct nested_ept_contexts *r) {
    if(r->poisoned) return false;
    for(struct nested_ept_context_item *p=r->items;p;p=p->next)
        if(p->tree.poisoned) {r->poisoned=true;return false;}
    return true;
}
bool nested_ept_contexts_init_config(struct nested_ept_contexts *r,
    const struct nested_sparse_ops *ops,void *context,unsigned maxphys,
    unsigned budget,bool evict,bool hardware_ad,
    nested_context_lifecycle_fn lifecycle,void *hook_context) {
    if(!r) return false;
    *r=(struct nested_ept_contexts){0};
    if(!ops||!ops->alloc||!ops->free||!ops->flush||maxphys<12||maxphys>48||!budget)
        return false;
    r->ops=*ops;r->allocator_context=context;r->maxphys=maxphys;
    r->max_resident=budget;r->evict=evict;r->initialized=true;
    r->hardware_ad=hardware_ad;r->lifecycle=lifecycle;r->hook_context=hook_context;
    return true;
}
bool nested_ept_contexts_init_mixed(struct nested_ept_contexts *r,
    const struct nested_sparse_ops *ops,void *context,unsigned maxphys,
    unsigned budget,bool evict,nested_context_lifecycle_fn lifecycle,void *hook_context) {
    if(!nested_ept_contexts_init_config(r,ops,context,maxphys,budget,evict,false,lifecycle,hook_context))
        return false;
    r->mixed_ad=true;
    return true;
}
bool nested_ept_contexts_init_budget(struct nested_ept_contexts *r,
    const struct nested_sparse_ops *ops,void *context,unsigned maxphys,
    unsigned budget,bool evict) {
    return nested_ept_contexts_init_config(r,ops,context,maxphys,budget,evict,false,NULL,NULL);
}
bool nested_ept_contexts_init(struct nested_ept_contexts *r,
    const struct nested_sparse_ops *ops,void *context,unsigned maxphys) {
    return nested_ept_contexts_init_budget(r,ops,context,maxphys,
        NESTED_EPT_CONTEXT_DEFAULT_BUDGET,false);
}
bool nested_ept_contexts_set_transactions(struct nested_ept_contexts *r,
 const struct nested_context_transactions *ops,void *context) {
 if(!r||!r->initialized||r->poisoned||r->items||r->count||r->active||r->tick||r->transactions||
    !ops||!ops->begin||!ops->end||!ops->reserve||!ops->cancel||!ops->attach||
    !ops->retain||!ops->publish||!ops->reset||!ops->retire) return false;
 r->transactions=ops;r->transaction_context=context;return true;
}
bool nested_ept_contexts_set_guards(struct nested_ept_contexts *r,
 const struct nested_context_guards *g,void *context) {
 if(!r||!r->initialized||r->poisoned||r->items||r->count||r->active||r->tick||r->guards||
    !g||!g->select||!g->pre_reset||!g->pre_retire)return false;
 r->guards=g;r->guard_context=context;return true;
}
static bool cancel_reservation(struct nested_ept_contexts *r,void *token) {
 if(!r->transactions) return true;
 if(r->transactions->cancel(r->transaction_context,token)) return true;
 r->poisoned=true;return false;
}
struct retire_bridge {struct nested_ept_contexts *registry;struct nested_ept_context_item *item;};
static bool retirement_receipt(void *context,const struct nested_ept_sparse *tree) {
 struct retire_bridge *b=context;
 if(tree!=&b->item->tree) return false;
 return b->registry->transactions->retire(b->registry->transaction_context,
     b->item->transaction_token,b->item);
}
static bool destroy_item_tree(struct nested_ept_contexts *r,struct nested_ept_context_item *p) {
 if(r->guards&&!r->guards->pre_retire(r->guard_context,p)){r->poisoned=true;return false;}
 struct retire_bridge bridge={r,p};
 return nested_ept_sparse_destroy_guarded(&p->tree,r->transactions?retirement_receipt:NULL,&bridge);
}
static void link_item(struct nested_ept_contexts *r,struct nested_ept_context_item *p) {
    p->next=r->items;r->items=p;r->count++;
}
static void unlink_free_item(struct nested_ept_contexts *r,struct nested_ept_context_item *p) {
    if(p->attachment_initialized) {
        r->lifecycle(r->hook_context,p,NESTED_CONTEXT_DESTROY);
        p->attachment_initialized=false;
    }
    struct nested_ept_context_item **link=&r->items;
    while(*link!=p) link=&(*link)->next;
    *link=p->next;
    struct nested_sparse_page meta=p->own_page;
    r->count--;
    r->ops.free(r->allocator_context,meta);
}
static enum nested_context_status select_inner(struct nested_ept_contexts *r,uint64_t key) {
    if(!r||!r->initialized) return NESTED_CONTEXT_INVALID;
    if(!healthy(r)) return NESTED_CONTEXT_POISONED;
    if(!valid_key(r,key)) return NESTED_CONTEXT_INVALID;
    if(r->tick==UINT64_MAX || r->count==~0u) return NESTED_CONTEXT_OVERFLOW;
    struct nested_ept_context_item *victim=NULL;
    for(struct nested_ept_context_item *p=r->items;p;p=p->next) {
        if(p->key==key) {
            if(r->guards&&!r->guards->select(r->guard_context,p)) {
                r->poisoned=true;return NESTED_CONTEXT_POISONED;
            }
            p->last_used=++r->tick;r->active=p;return NESTED_CONTEXT_OK;
        }
        if(p!=r->active&&!p->pins&&(!victim||p->last_used<victim->last_used)) victim=p;
    }
    if(r->count>=r->max_resident && (!r->evict||!victim)) return NESTED_CONTEXT_LIMIT;
    if(r->count<r->max_resident) victim=NULL;
    /* Peak requirement: candidate metadata + root pair must be allocated
     * BEFORE releasing a victim. OOM never destroys a still-usable cache. */
    void *token=NULL;
    if(r->transactions && (!r->transactions->reserve(r->transaction_context,key,&token)||!token))
        return NESTED_CONTEXT_OOM;
    struct nested_sparse_page meta;
    if(!r->ops.alloc(r->allocator_context,&meta))
        return cancel_reservation(r,token)?NESTED_CONTEXT_OOM:NESTED_CONTEXT_POISONED;
    if(!meta.view||((uintptr_t)meta.view&4095)||(meta.hpa&4095)||
       meta.hpa>=(UINT64_C(1)<<r->maxphys)) {
        r->ops.free(r->allocator_context,meta);
        return cancel_reservation(r,token)?NESTED_CONTEXT_OOM:NESTED_CONTEXT_POISONED;
    }
    struct nested_ept_context_item *candidate=meta.view;
    *candidate=(struct nested_ept_context_item){.own_page=meta,.key=key,.transaction_token=token};
    if(!nested_ept_sparse_init_mode(&candidate->tree,&r->ops,r->allocator_context,r->maxphys,(key&0x40)!=0)) {
        if(candidate->tree.root) {
            if(r->transactions) (void)r->transactions->attach(r->transaction_context,token,candidate,false);
            if(r->transactions) r->transactions->retain(r->transaction_context,token,candidate);
            link_item(r,candidate);r->poisoned=true;
            return NESTED_CONTEXT_FLUSH_FAILED;
        }
        r->ops.free(r->allocator_context,meta);
        return cancel_reservation(r,token)?NESTED_CONTEXT_OOM:NESTED_CONTEXT_POISONED;
    }
    if(r->transactions&&!r->transactions->attach(r->transaction_context,token,candidate,true)) {
        r->transactions->retain(r->transaction_context,token,candidate);
        link_item(r,candidate);r->poisoned=true;return NESTED_CONTEXT_POISONED;
    }
    if(r->lifecycle) {
        r->lifecycle(r->hook_context,candidate,NESTED_CONTEXT_INIT);
        candidate->attachment_initialized=true;
    }
    if(victim) {
        if(!destroy_item_tree(r,victim)) {
            /* Candidate also owns a potentially cached EPTP: do not discard
             * it or leak it when victim invalidation fails. Active unchanged. */
            if(r->transactions) r->transactions->retain(r->transaction_context,token,candidate);
            link_item(r,candidate);r->poisoned=true;
            return NESTED_CONTEXT_FLUSH_FAILED;
        }
        unlink_free_item(r,victim);
    }
    link_item(r,candidate);
    if(r->transactions&&!r->transactions->publish(r->transaction_context,token,candidate)) {
        r->transactions->retain(r->transaction_context,token,candidate);
        r->poisoned=true;return NESTED_CONTEXT_POISONED;
    }
    candidate->last_used=++r->tick;r->active=candidate;
    return NESTED_CONTEXT_OK;
}
struct nested_ept_sparse *nested_ept_contexts_active(struct nested_ept_contexts *r) {
    if(!r||!r->initialized||!healthy(r)||!r->active) return NULL;
    return &r->active->tree;
}
enum nested_sparse_lookup nested_ept_contexts_lookup(struct nested_ept_contexts *r,
    uint64_t gpa,uint8_t access,struct nested_ept_result *result) {
    if(!r||!r->initialized) return NESTED_SPARSE_BAD_LOOKUP;
    if(!healthy(r)) return NESTED_SPARSE_LOOKUP_POISONED;
    struct nested_ept_sparse *tree=nested_ept_contexts_active(r);
    if(!tree) return NESTED_SPARSE_MISS;
    return nested_ept_sparse_lookup(tree,gpa,access,result);
}
static struct nested_ept_context_item *pin_inner(struct nested_ept_contexts *r) {
    if(!r||!r->initialized||!healthy(r)||!r->active||r->active->pins==UINT32_MAX) return NULL;
    r->active->pins++;return r->active;
}
static bool unpin_inner(struct nested_ept_contexts *r,struct nested_ept_context_item *item) {
    if(!r||!r->initialized||!item) return false;
    for(struct nested_ept_context_item *p=r->items;p;p=p->next) if(p==item) {
        if(!p->pins) return false;
        p->pins--;return true;
    }
    return false;
}
static void clear_pins_inner(struct nested_ept_contexts *r) {
    if(!r||!r->initialized) return;
    for(struct nested_ept_context_item *p=r->items;p;p=p->next) p->pins=0;
}
static enum nested_context_status invalidate_inner(struct nested_ept_contexts *r,
    uint64_t type,uint64_t key) {
    if(!r||!r->initialized) return NESTED_CONTEXT_INVALID;
    if(!healthy(r)) return NESTED_CONTEXT_POISONED;
    if((type!=1&&type!=2)||(type==1&&!valid_key(r,key))) return NESTED_CONTEXT_INVALID;
    for(struct nested_ept_context_item *p=r->items;p;p=p->next) {
        if(type==1&&(p->key&ROOT_MASK)!=(key&ROOT_MASK)) continue;
        if(r->guards&&!r->guards->pre_reset(r->guard_context,p)) {
            r->poisoned=true;return NESTED_CONTEXT_POISONED;
        }
        if(!nested_ept_sparse_reset(&p->tree)) {
            r->poisoned=true;
            return p->tree.poisoned?NESTED_CONTEXT_FLUSH_FAILED:NESTED_CONTEXT_POISONED;
        }
        /* Notification only: reset already freed lower pages and advanced
         * generation. Failure poisons, never rolls back that ownership change. */
        if(r->transactions&&!r->transactions->reset(r->transaction_context,p->transaction_token,p)) {
            r->poisoned=true;return NESTED_CONTEXT_POISONED;
        }
        if(p->attachment_initialized) r->lifecycle(r->hook_context,p,NESTED_CONTEXT_RESET);
    }
    return NESTED_CONTEXT_OK;
}
static bool destroy_inner(struct nested_ept_contexts *r) {
    if(!r) return false;
    if(!r->initialized) return true;
    for(struct nested_ept_context_item *p=r->items;p;p=p->next) if(p->pins) return false;
    bool success=true;
    r->active=NULL;
    struct nested_ept_context_item *p=r->items;
    while(p) {
        struct nested_ept_context_item *next=p->next;
        if(!destroy_item_tree(r,p)) success=false;
        else unlink_free_item(r,p);
        p=next;
    }
    if(success) *r=(struct nested_ept_contexts){0};
    else r->poisoned=true;
    return success;
}

enum nested_context_status nested_ept_contexts_select(struct nested_ept_contexts *r,uint64_t key) {
 if(!r||!r->initialized) return NESTED_CONTEXT_INVALID;
 const struct nested_context_transactions *ops=r->transactions;void *context=r->transaction_context;
 if(ops&&!ops->begin(context,NESTED_CONTEXT_SELECT)) return NESTED_CONTEXT_INVALID;
 enum nested_context_status status=select_inner(r,key);
 if(ops) ops->end(context,NESTED_CONTEXT_SELECT);
 return status;
}
enum nested_context_status nested_ept_contexts_invalidate(struct nested_ept_contexts *r,uint64_t type,uint64_t key) {
 if(!r||!r->initialized) return NESTED_CONTEXT_INVALID;
 const struct nested_context_transactions *ops=r->transactions;void *context=r->transaction_context;
 if(ops&&!ops->begin(context,NESTED_CONTEXT_INVALIDATE)) return NESTED_CONTEXT_INVALID;
 enum nested_context_status status=invalidate_inner(r,type,key);
 if(ops) ops->end(context,NESTED_CONTEXT_INVALIDATE);
 return status;
}
bool nested_ept_contexts_destroy(struct nested_ept_contexts *r) {
 if(!r) return false;
 const struct nested_context_transactions *ops=r->transactions;void *context=r->transaction_context;
 if(ops&&!ops->begin(context,NESTED_CONTEXT_DISPOSE)) return false;
 bool status=destroy_inner(r);
 if(ops) ops->end(context,NESTED_CONTEXT_DISPOSE);
 return status;
}

struct nested_ept_context_item *nested_ept_contexts_pin_active(struct nested_ept_contexts *r) {
 if(!r||!r->initialized) return NULL;
 const struct nested_context_transactions *ops=r->transactions;void *context=r->transaction_context;
 if(ops&&!ops->begin(context,NESTED_CONTEXT_PIN)) return NULL;
 struct nested_ept_context_item *item=pin_inner(r);
 if(ops) ops->end(context,NESTED_CONTEXT_PIN);
 return item;
}
bool nested_ept_contexts_unpin(struct nested_ept_contexts *r,struct nested_ept_context_item *p) {
 if(!r||!r->initialized) return false;
 const struct nested_context_transactions *ops=r->transactions;void *context=r->transaction_context;
 if(ops&&!ops->begin(context,NESTED_CONTEXT_UNPIN)) return false;
 bool ok=unpin_inner(r,p);
 if(ops) ops->end(context,NESTED_CONTEXT_UNPIN);
 return ok;
}
void nested_ept_contexts_release_all_after_clear(struct nested_ept_contexts *r) {
 if(!r||!r->initialized) return;
 const struct nested_context_transactions *ops=r->transactions;void *context=r->transaction_context;
 if(ops&&!ops->begin(context,NESTED_CONTEXT_CLEAR_PINS)) return;
 clear_pins_inner(r);
 if(ops) ops->end(context,NESTED_CONTEXT_CLEAR_PINS);
}
