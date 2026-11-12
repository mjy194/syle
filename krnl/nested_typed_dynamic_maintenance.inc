/* N38C-B: real two-owner, budget-two A/B/C churn around WB -> UC -> WB.
 * Stable tokens and retirement receipts belong to the target backing lease,
 * never to an enumeration index or a reusable physical root address. */
#include "cache_maintenance.h"
#include "cache_mp_transition.h"
#include "domain_typed_alias_plan.h"
#include "nested_owner_dynamic_typed.h"
#include "typed_root_registry.h"
#ifdef VMX_NESTED_OWNER_ROOT_SCOPED_TEST
#include "typed_root_local_provider.h"
#endif

#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
#include "typed_root_overlap.h"
#endif

#define TYPED_DYNAMIC_SLOTS 16u
#define TYPED_DYNAMIC_FIXED 5u
#define TYPED_DYNAMIC_ROOTS 9u
#define TYPED_DYNAMIC_SNAPSHOT 80u
#define TYPED_DYNAMIC_BACKING_LIFETIME UINT64_C(0x38c0000000000001)
enum typed_command { TYPED_RESET=1,TYPED_INVEPT,TYPED_TLB,TYPED_CACHE };
struct typed_root_phase_ack {
    struct td_identity identity;
    struct trl_freeze freeze;
    uint64_t eptp,epoch;
    unsigned cpu,phase;
    bool complete;
};
static struct {
    struct typed_directory directory;
    struct typed_root_lease lease;
    struct typed_root_registry bridges[2];
    struct nested_owner_dynamic_typed owners[2];
    struct tr_registry_slot slots[2][TYPED_DYNAMIC_SLOTS];
    unsigned slots_used[2];
#ifdef VMX_NESTED_OWNER_ROOT_SCOPED_TEST
    unsigned provider_receipts[2];
    struct tr_local_provider local_provider[2];
    struct tr_local_completed completed[2][TYPED_DYNAMIC_SLOTS];
#endif
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    struct tr_overlap overlap[2];
    struct tr_overlap_counts overlap_before[2]; /* peer-owned, never read live remotely */
    struct tr_registry_slot *held_slot[2],*peer_slot[2];
    bool overlap_entered[2];
    uint32_t lock_owner,teardown_turn;
#endif
    struct td_record fixed_records[TYPED_DYNAMIC_FIXED];
    struct td_identity fixed_ids[TYPED_DYNAMIC_FIXED];
    struct td_receipt fixed_receipts[TYPED_DYNAMIC_FIXED];
    struct trl_binding fixed_bindings[TYPED_DYNAMIC_FIXED];
    struct trl_snapshot snapshot[TYPED_DYNAMIC_SNAPSHOT];
    size_t snapshot_count,receipt_count;
    struct trl_binding *live_bindings[TYPED_DYNAMIC_ROOTS];
    struct td_identity live_ids[TYPED_DYNAMIC_ROOTS];
    struct domain_typed_root_binding roots[TYPED_DYNAMIC_ROOTS];
    unsigned root_count;
    struct trl_freeze freeze,plan_freeze;
    struct typed_root_phase_ack root_ack[2][TYPED_DYNAMIC_ROOTS][2];
    struct cache_mp_transition transition;
    struct cache_cpu_snapshot snapshots[2];
    struct cache_page_lease hostlease,initial_host;
    struct domain_typed_request request;
    struct domain_typed_inventory inventory;
    struct domain_typed_alias_plan plan;
    struct domain_typed_alias_workspace workspace;
    struct nested_domain_maintenance_ticket domain_ticket;
    uint64_t lifetime,flush_serial,transition_serial,plan_epoch,audit_version,audit_lease_version;
    uint64_t sequence,command_epoch,ack_sequence;
    unsigned command,command_root_phase;
    uint32_t metadata_lock,fault,turn,stopped[4][2],released[4],retired[2];
    uint32_t final_cache[2];
    uint64_t source_epoch;
    bool configured,initialized,source_registered,terminal_audit;
} typed_runtime;

#ifdef VMX_NESTED_ROOT_CATALOG_TEST
/* Diagnostic provisioning data only; the catalog/owner APIs do not encode
 * these addresses or root count. This bounded fixture still owns 63 pages. */
#define TYPED_CATALOG_ROOTS 3u
static struct nested_root_catalog typed_catalog;
static struct nrc_span typed_catalog_spans[EPT_GUEST_PAGES];
static struct nrc_root typed_catalog_roots[TYPED_CATALOG_ROOTS];
static uint64_t *typed_catalog_type_leaves[TYPED_CATALOG_ROOTS];
static bool typed_catalog_prepare(void){
    if(!nested_domain_memory_typed_ready(&domain_memory))return false;
    struct nrc_identity identity={domain_dispatch.domain.id,domain_dispatch.domain.incarnation,0x40,UINT64_C(0x4000000000000001)};
    if(!nrc_init(&typed_catalog,identity,48,typed_catalog_spans,EPT_GUEST_PAGES,
        typed_catalog_roots,TYPED_CATALOG_ROOTS,nested_owner_catalog_authority))return false;
    const uint64_t *pt=paging_phys_to_virt(domain_memory.root01[0][3]);
    for(unsigned n=1;n<=EPT_GUEST_PAGES;n++){
        uint64_t hpa=domain_memory.backing.pages[n-1];bool table=n>=16&&n<=27;
        struct nrc_span span={.identity=identity,.object=n,.lifetime=UINT64_C(0x4000000000010000)+n,
            .gpa=(uint64_t)n*4096,.hpa=hpa,.bytes=4096,.table_view=table?paging_phys_to_virt(hpa):NULL,
            .permissions=pt[n]&7,.allowed_types=hpa==domain_memory.backing.pages[14]?NRC_TYPE_WB|NRC_TYPE_UC:NRC_TYPE_WB,
            .role=table?NRC_TABLE:NRC_DATA,.ignore_pat=true};
        if(!nrc_add_span(&typed_catalog,span))return false;
    }
    const uint64_t keys[TYPED_CATALOG_ROOTS]={0x1001e,0x1401e,0x1801e};
    for(unsigned k=0;k<TYPED_CATALOG_ROOTS;k++){
        struct nrc_root root={identity,k+1,0,1,UINT64_C(0x4000000000020000)+k,keys[k],keys[k]&~UINT64_C(4095)};
        if(!nrc_add_root(&typed_catalog,root))return false;
        typed_catalog_type_leaves[k]=(uint64_t *)paging_phys_to_virt(domain_memory.backing.pages[18+4*k])+15;
    }
    return nrc_publish(&typed_catalog);
}
#endif
static size_t typed_source_count(void){
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
    return typed_catalog.root_count;
#else
    return 3;
#endif
}

static void typed_fatal(void *opaque){
    (void)opaque;
    /* No unlocked poison/lease writes: a peer may still be in a callback. */
    __atomic_store_n(&typed_runtime.fault,1,__ATOMIC_RELEASE);
    nested_domain_dispatch_markfail(&domain_dispatch);
}
static uint64_t typed_clock(void){uint32_t a,d;__asm__ volatile("rdtsc":"=a"(a),"=d"(d));return (uint64_t)a|((uint64_t)d<<32);}
static bool typed_failed(void){
    return __atomic_load_n(&typed_runtime.fault,__ATOMIC_ACQUIRE)||
        __atomic_load_n(&domain_dispatch.fault_latch,__ATOMIC_ACQUIRE);
}
static bool typed_wait32(uint32_t *word){uint64_t start=typed_clock();
    while(!__atomic_load_n(word,__ATOMIC_ACQUIRE)){
        if(typed_failed()||typed_clock()-start>UINT64_C(30000000000))return false;
        __asm__ volatile("pause");
    }return !typed_failed();
}
static bool typed_meta_lock(void){
    for(unsigned n=0;n<1000000;n++){uint32_t zero=0;
        if(__atomic_compare_exchange_n(&typed_runtime.metadata_lock,&zero,1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)){
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
            __atomic_store_n(&typed_runtime.lock_owner,cpu_id()+1,__ATOMIC_RELEASE);
#endif
            return true;
        }
        __asm__ volatile("pause");
    }typed_fatal(0);return false;
}
static void typed_meta_unlock(void){
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    __atomic_store_n(&typed_runtime.lock_owner,0,__ATOMIC_RELEASE);
#endif
    __atomic_store_n(&typed_runtime.metadata_lock,0,__ATOMIC_RELEASE);
}
/* Bridge hooks cannot report failure. A timeout cannot return as if it owned
 * the lock: latch failure and stop this owner, retaining every allocation. */
static void typed_bridge_lock(void *unused){
    (void)unused;if(typed_meta_lock())return;
    for(;;)__asm__ volatile("cli;hlt":::"memory");
}
static void typed_bridge_unlock(void *unused){(void)unused;typed_meta_unlock();}
static bool typed_identity_equal(struct td_identity a,struct td_identity b){
    return a.domain==b.domain&&a.incarnation==b.incarnation&&a.root_id==b.root_id&&
        a.root_incarnation==b.root_incarnation&&a.allocator_lifetime==b.allocator_lifetime&&a.owner==b.owner;
}
#ifndef VMX_NESTED_OWNER_ROOT_SCOPED_TEST
static bool typed_ticket_equal(struct td_retire_ticket a,struct td_retire_ticket b){
    return typed_identity_equal(a.identity,b.identity)&&a.full_eptp==b.full_eptp&&a.hpa==b.hpa&&
        a.cache_generation==b.cache_generation&&a.history==b.history&&a.type_epoch==b.type_epoch&&a.nonce==b.nonce;
}
#endif
static bool typed_freeze_equal(struct trl_freeze a,struct trl_freeze b){
    return a.directory.domain==b.directory.domain&&a.directory.incarnation==b.directory.incarnation&&
        a.directory.nonce==b.directory.nonce&&a.directory.version==b.directory.version&&
        a.directory.type_epoch==b.directory.type_epoch&&a.lease_version==b.lease_version&&a.type==b.type&&
        a.backing.hpa==b.backing.hpa&&a.backing.allocator_lifetime==b.backing.allocator_lifetime&&
        a.backing.dedicated_data_wb==b.backing.dedicated_data_wb;
}
static bool typed_guard(void *opaque){
    struct nested_parallel_runtime *r=opaque;uint64_t cr0;uint32_t a,d;
    if(!r||r->cpu_index>1||typed_failed())return false;
    /* Bind runs before checkpoint 2. Establish this owner's initial hardware
     * policy then, and recapture it at the stopped bootstrap checkpoint. */
    if(!typed_runtime.snapshots[r->cpu_index].pat&&
       !cache_maintenance_precheck(domain_memory.backing.pages[14],&typed_runtime.snapshots[r->cpu_index]))return false;
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));
    __asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(0x277));
    return r&&r->cpu_index<2&&!typed_failed()&&!(cr0&UINT64_C(0x60000000))&&
        ((uint64_t)a|((uint64_t)d<<32))==typed_runtime.snapshots[r->cpu_index].pat;
}
static bool typed_invept(uint64_t eptp){
    struct {uint64_t eptp,zero;} __attribute__((aligned(16))) descriptor={eptp,0};
    unsigned char fail;uint64_t type=1;
    __asm__ volatile("invept %1,%2;setna %0":"=qm"(fail):"m"(descriptor),"r"(type):"cc","memory");return !fail;
}
/* Called under the bridge's short metadata lock, before root allocation. */
static bool typed_provider_reserve(void *opaque,struct tr_registry_slot **out,uint64_t *life){
    struct nested_parallel_runtime *r=opaque;unsigned cpu=r->cpu_index;
    if(cpu>1||!out||!life||typed_failed()||typed_runtime.slots_used[cpu]>=TYPED_DYNAMIC_SLOTS||
       typed_runtime.lifetime==UINT64_MAX)return false;
    *out=&typed_runtime.slots[cpu][typed_runtime.slots_used[cpu]++];
    *life=++typed_runtime.lifetime;return true;
}
#ifdef VMX_NESTED_OWNER_ROOT_SCOPED_TEST
static bool typed_provider_lock(void *unused){(void)unused;return typed_meta_lock();}
static unsigned typed_provider_cpu(void *unused){(void)unused;return cpu_id();}
static bool typed_provider_healthy(void *unused){(void)unused;return !typed_failed();}
static bool typed_provider_invalidate(void *opaque,uint64_t eptp,struct tr_local_invalidation *out){
    struct nested_parallel_runtime *r=opaque;unsigned cpu=cpu_id();
    if(!r||cpu>1||cpu!=r->cpu_index||typed_failed()||!typed_invept(eptp)||cpu_id()!=cpu)return false;
    *out=(struct tr_local_invalidation){.full_eptp=eptp,.acked=UINT64_C(1)<<cpu,.cpu=cpu,.success=true};
    return true;
}
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
static uint64_t typed_overlap_now(void *unused){(void)unused;return typed_clock();}
static void typed_overlap_relax(void *unused){(void)unused;__asm__ volatile("pause");}
static bool typed_overlap_lock_owned(void *unused){
    (void)unused;return __atomic_load_n(&typed_runtime.lock_owner,__ATOMIC_ACQUIRE)==cpu_id()+1;
}
static bool typed_overlap_wait(uint32_t *word){
    const struct tr_overlap_wait wait={typed_overlap_now,typed_overlap_relax,
        typed_provider_healthy,typed_overlap_lock_owned,NULL,UINT64_C(30000000000)};
    return tr_overlap_wait_for(word,&wait);
}
static struct tr_overlap_counts typed_overlap_counts(struct nested_parallel_runtime *r){
    return (struct tr_overlap_counts){r->sparse.creations,r->sparse.evictions,
        r->sparse.misses,r->sparse.overlap_switch.completed,r->sparse.overlap_switch.sequence};
}
/* Caller owns metadata lock. Only durable stable records, never peer pool
 * items. Final records remain auditable after all allocator pages are gone. */
static bool typed_overlap_receipt(unsigned cpu,struct tr_registry_slot *slot,struct tr_overlap_receipt *out){
    if(!tr_local_completed_matches(&typed_runtime.local_provider[cpu],slot))return false;
    size_t index=0;for(;index<TYPED_DYNAMIC_SLOTS;index++)if(slot==&typed_runtime.slots[cpu][index])break;
    if(index==TYPED_DYNAMIC_SLOTS)return false;
    *out=(struct tr_overlap_receipt){typed_runtime.completed[cpu][index],slot->directory.receipt,slot->evidence_version};
    return true;
}
static bool typed_overlap_hold(struct nested_parallel_runtime *r,struct tr_registry_slot *slot,
    struct tr_registry_retire_scope scope){
    unsigned cpu=r->cpu_index,phase=scope.ticket.type_epoch==1?0:1;
    /* Exact first eviction in each chosen round, not a teardown callback. */
    if(scope.ticket.type_epoch>2||cpu!=phase||r->sparse.evictions!=(phase?2u:0u))return true;
    if(r->sparse.selecting12!=(phase?0x1401eu:0x1801eu)||
       r->sparse.selected12!=(phase?0x1801eu:0x1401eu)||cpu_id()!=cpu||!typed_meta_lock())return false;
    size_t index=0;for(;index<TYPED_DYNAMIC_SLOTS;index++)if(slot==&typed_runtime.slots[cpu][index])break;
    bool ok=index<TYPED_DYNAMIC_SLOTS&&td_retire_pending(&typed_runtime.directory,
        &typed_runtime.bridges[cpu].directory.operation,scope.operation_nonce,&slot->directory.record,
        scope.ticket.identity,scope.ticket)&&!typed_runtime.directory.closed&&
        !typed_runtime.directory.sealed&&typed_runtime.lease.type_epoch==scope.ticket.type_epoch;
    struct tr_overlap_hold held={0};
    if(ok){held=(struct tr_overlap_hold){typed_runtime.completed[cpu][index],typed_runtime.directory.version,phase,cpu};
        typed_runtime.held_slot[phase]=slot;}
    /* Publish the immutable copy after releasing the short metadata lock.
     * The admitted bridge operation continues to retain victim and candidate. */
    typed_meta_unlock();
    return ok&&tr_overlap_publish_hold(&typed_runtime.overlap[phase],held)&&
        typed_overlap_wait(&typed_runtime.overlap[phase].progress)&&cpu_id()==cpu;
}
static bool typed_overlap_entry(struct nested_parallel_runtime *r){
    unsigned cpu=r->cpu_index;
    /* Release flags are the synchronized epoch publication, not an unlocked
     * shared lease read while the other owner may update membership. */
    unsigned phase=__atomic_load_n(&typed_runtime.released[0],__ATOMIC_ACQUIRE)?1:0;
    if(__atomic_load_n(&typed_runtime.released[1],__ATOMIC_ACQUIRE)||cpu==phase||typed_runtime.overlap_entered[phase])return true;
    typed_runtime.overlap_before[phase]=typed_overlap_counts(r);
    if(!typed_overlap_wait(&typed_runtime.overlap[phase].held))return false;
    const struct tr_overlap_hold *held=&typed_runtime.overlap[phase].hold;
    if(held->phase!=phase||held->cpu!=phase||held->completed.scope.ticket.type_epoch!=phase+1)return false;
    typed_runtime.overlap_entered[phase]=true;return true;
}
static bool typed_overlap_checkpoint(struct nested_parallel_runtime *r,unsigned phase){
    if(phase>1)return true;
    unsigned cpu=r->cpu_index;struct tr_overlap *overlap=&typed_runtime.overlap[phase];
    if(!__atomic_load_n(&overlap->held,__ATOMIC_ACQUIRE)||!typed_meta_lock())return false;
    bool ok=false;
    if(cpu==phase){
        struct tr_overlap_receipt retired={0};
        ok=typed_overlap_receipt(cpu,typed_runtime.held_slot[phase],&retired);
        typed_meta_unlock();return ok&&tr_overlap_finish(overlap,retired);
    }
    struct tr_overlap_peer peer={.before=typed_runtime.overlap_before[phase],
        .after=typed_overlap_counts(r),.switched=r->sparse.overlap_switch,.phase=phase,.cpu=cpu};
    if(typed_runtime.overlap_entered[phase])
        for(struct tr_registry_slot *slot=typed_runtime.bridges[cpu].slots;slot;slot=slot->next)
            if(slot->directory.forgotten&&slot->directory.receipt.type_epoch==phase+1&&
               slot->directory.receipt.commit_version>overlap->hold.version){
                ok=typed_overlap_receipt(cpu,slot,&peer.retired);
                if(ok)typed_runtime.peer_slot[phase]=slot;
                break;
            }
    typed_meta_unlock();return ok&&tr_overlap_publish_peer(overlap,peer);
}
static bool typed_overlap_final(void){
    for(unsigned phase=0;phase<2;phase++){
        struct tr_overlap *o=&typed_runtime.overlap[phase];struct tr_overlap_receipt held={0},peer={0};
        if(!tr_overlap_valid(o)||!typed_overlap_receipt(phase,typed_runtime.held_slot[phase],&held)||
           !typed_overlap_receipt(phase^1,typed_runtime.peer_slot[phase],&peer)||
           !tr_overlap_receipt_equal(held,o->final)||
           !tr_overlap_receipt_equal(peer,o->peer.retired))return false;
    }return true;
}
#endif
static bool typed_provider_root_receipt(void *opaque,struct tr_registry_slot *slot,
    struct tr_registry_retire_scope scope,struct tr_registry_root_evidence *out){
    struct nested_parallel_runtime *r=opaque;
    if(!r||r->cpu_index>1)return false;
    bool ok=tr_local_provider_flush(&typed_runtime.local_provider[r->cpu_index],slot,scope,out);
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    if(ok)ok=typed_overlap_hold(r,slot,scope);
#endif
    if(!ok)typed_fatal(r); /* Retain every tree/backing on any missing hardware proof. */
    return ok;
}
#else
/* guarded_destroy has already completed this owner's actual local INVEPT.
 * The bridge invokes us unlocked, and validates the same version on return.
 * This narrow serial-owner fixture permits no remote root history. */
static bool typed_provider_receipt(void *opaque,struct tr_registry_slot *slot,
    struct td_retire_ticket ticket,uint64_t version,struct tr_registry_flush_evidence *out){
    struct nested_parallel_runtime *r=opaque;unsigned cpu=r->cpu_index;
    if(cpu>1||!slot||!out||!typed_meta_lock())return false;
    bool ok=!typed_failed()&&slot->bridge==&typed_runtime.bridges[cpu]&&
        slot->directory.identity.owner==cpu&&ticket.identity.owner==cpu&&
        !(ticket.history&~(UINT64_C(1)<<cpu))&&
        typed_ticket_equal(ticket,slot->directory.retirement)&&
        slot->directory.record.state==TD_RETIRING&&slot->directory.item&&
        !slot->directory.item->pins&&!slot->directory.record.pins&&!slot->directory.record.refills&&
        typed_runtime.directory.version==version&&typed_runtime.flush_serial!=UINT64_MAX;
    if(ok)*out=(struct tr_registry_flush_evidence){ticket,version,ticket.history,++typed_runtime.flush_serial};
    typed_meta_unlock();return ok;
}
#endif
static struct trl_binding *typed_binding(struct td_identity id){
    for(struct trl_binding *b=typed_runtime.lease.bindings;b;b=b->next)
        if(typed_identity_equal(b->token.identity,id))return b;
    return NULL;
}
static bool typed_fixed_init(void){
    struct typed_directory *d=&typed_runtime.directory;struct typed_root_lease *l=&typed_runtime.lease;
    struct td_operation op={0};uint64_t n=0;bool ok=td_begin(d,&op,&n);
    for(unsigned i=0;ok&&i<TYPED_DYNAMIC_FIXED;i++){
        unsigned owner=i<2?i:i<4?i-2:0;
        uint64_t h=i<2?domain_memory.root01[i][0]:i<4?domain_memory.root02[i-2][0]:domain_memory.backing.tables[0];
        struct td_record *r=&typed_runtime.fixed_records[i];struct trl_binding *b=&typed_runtime.fixed_bindings[i];
        struct td_identity *id=&typed_runtime.fixed_ids[i];uint64_t life=++typed_runtime.lifetime;
        ok=trl_reserve(l,&op,n,b,owner,life)&&td_reserve(d,&op,n,r,owner,life,id)&&
            trl_bind_candidate(l,&op,n,b,r,*id)&&td_attach(d,&op,n,r,*id,h|0x1e,h,1,0)&&
            trl_attach(l,&op,n,b)&&trl_publish_ready(l,&op,n,b)&&td_publish(d,&op,n,r,*id)&&trl_publish(l,&op,n,b);
    }
    /* Three immutable EPT12 trees are durable external refill sources, unlike
     * the tree-owned cache records, which are covered by reset/retirement. */
    for(size_t k=0;ok&&k<typed_source_count();k++)ok=td_refill_add(d,&op,n,&typed_runtime.fixed_records[0],typed_runtime.fixed_ids[0]);
    if(op.live){bool ended=td_end(d,&op,n);ok=ok&&ended;}
    if(ok){typed_runtime.source_registered=true;typed_runtime.source_epoch=l->type_epoch;}
    return ok;
}
static bool typed_sources(bool add){
    if(!typed_meta_lock())return false;
    struct typed_directory *d=&typed_runtime.directory;struct td_record *r=&typed_runtime.fixed_records[0];
    struct td_operation op={0};uint64_t n=0;
    bool ok=typed_runtime.source_registered!=add&&r->refills==(add?0:typed_source_count())&&td_begin(d,&op,&n);
    for(size_t k=0;ok&&k<typed_source_count();k++)ok=add?td_refill_add(d,&op,n,r,r->identity):td_refill_drop(d,&op,n,r,r->identity);
    if(op.live){bool ended=td_end(d,&op,n);ok=ok&&ended;}
    if(ok){typed_runtime.source_registered=add;if(add)typed_runtime.source_epoch=typed_runtime.lease.type_epoch;}
    typed_meta_unlock();return ok;
}
static bool typed_table(void *unused,uint64_t h,const uint64_t **view){
    (void)unused;bool owned=false;
    for(unsigned l=0;l<4;l++){
        if(h==domain_memory.backing.tables[l])owned=true;
        for(unsigned cpu=0;cpu<2;cpu++)if(h==domain_memory.root01[cpu][l]||h==domain_memory.root02[cpu][l])owned=true;
    }
    for(unsigned cpu=0;cpu<2;cpu++)for(unsigned p=0;p<NESTED_OWNER_POOL_PAGES;p++)
        if(nested_runs[cpu].sparse.slots[p].live&&h==nested_runs[cpu].sparse.pages[p].hpa)owned=true;
    if(!owned||!h||(h&4095)||h==domain_memory.backing.pages[14]||!vmx_memory_region_wb(h))return false;
    *view=paging_phys_to_virt(h);return *view!=0;
}
/* Both owners are stopped for these copies. A sealed transition uses only
 * scan_allowed entries; receipt-only identities never reach the table walk. */
static bool typed_collect(bool sealed){
    if(!typed_meta_lock())return false;
    struct typed_directory *d=&typed_runtime.directory;
    bool ok=!typed_failed()&&!d->operations&&!d->poisoned&&!typed_runtime.lease.poisoned;
    typed_runtime.root_count=0;typed_runtime.snapshot_count=0;typed_runtime.receipt_count=0;
    if(ok&&sealed)ok=trl_snapshot_read(&typed_runtime.lease,typed_runtime.freeze,
        typed_runtime.snapshot,TYPED_DYNAMIC_SNAPSHOT,&typed_runtime.snapshot_count);
    if(ok&&sealed)for(size_t n=0;n<typed_runtime.snapshot_count;n++){
        struct trl_snapshot *s=&typed_runtime.snapshot[n];
        if(!s->scan_allowed){
            if(s->root.state!=TD_RETIRED||!s->receipt_version||!s->receipt_type_epoch){ok=false;break;}
            if(s->root.receipt)typed_runtime.receipt_count++;
            continue; /* Never dereference an old receipt HPA. */
        }
        struct trl_binding *b=typed_binding(s->root.identity);
        unsigned q=typed_runtime.root_count;
        if(q>=TYPED_DYNAMIC_ROOTS||!b||!b->record||!b->token_live||s->root.receipt||
           !s->root.attached||!s->root.cache_generation||s->root.refills||
           s->root.full_eptp!=b->full_eptp||s->root.hpa!=b->hpa||
           (s->root.history&~UINT64_C(3))){ok=false;break;}
        typed_runtime.live_bindings[q]=b;typed_runtime.live_ids[q]=s->root.identity;
        typed_runtime.roots[q]=(struct domain_typed_root_binding){s->root.identity.root_id,
            s->root.identity.root_incarnation,s->root.full_eptp,s->root.history};
        typed_runtime.root_count++;
    }
    if(ok&&!sealed)for(struct td_record *r=d->records;r;r=r->next){
        if(r->state==TD_RETIRED)continue;
        unsigned q=typed_runtime.root_count;struct trl_binding *b=typed_binding(r->identity);
        if(q>=TYPED_DYNAMIC_ROOTS||!b||!b->token_live||b->record!=r||!r->attached||
           r->state!=TD_LIVE||!r->cache_generation||(r->history&~UINT64_C(3))){ok=false;break;}
        typed_runtime.live_bindings[q]=b;typed_runtime.live_ids[q]=r->identity;
        typed_runtime.roots[q]=(struct domain_typed_root_binding){r->identity.root_id,
            r->identity.root_incarnation,r->full_eptp,r->history};
        typed_runtime.root_count++;
    }
    if(ok){
        typed_runtime.request=(struct domain_typed_request){typed_runtime.roots,typed_runtime.root_count,48,
            domain_memory.backing.pages[14],8192};
        typed_runtime.audit_version=d->version;typed_runtime.audit_lease_version=typed_runtime.lease.version;
    }
    typed_meta_unlock();return ok&&typed_runtime.root_count>=TYPED_DYNAMIC_FIXED;
}
static bool typed_scan(unsigned type){
    if(domain_typed_inventory_audit(&typed_runtime.request,typed_table,0,&typed_runtime.workspace.scan,
        &typed_runtime.inventory)!=DOMAIN_TYPED_OK)return false;
    for(unsigned i=0;i<typed_runtime.inventory.alias_count;i++)
        if(typed_runtime.inventory.aliases[i].memory_type!=type)return false;
    if(!typed_meta_lock())return false;
    bool ok=!typed_runtime.directory.operations&&!typed_runtime.directory.poisoned&&
        typed_runtime.directory.version==typed_runtime.audit_version&&
        typed_runtime.lease.version==typed_runtime.audit_lease_version;
    typed_meta_unlock();return ok;
}
static bool typed_plan_valid(void){
    if(!typed_meta_lock())return false;
    bool ok=trl_freeze_valid(&typed_runtime.lease,typed_runtime.freeze)&&
        typed_freeze_equal(typed_runtime.freeze,typed_runtime.plan_freeze)&&
        typed_runtime.plan_epoch==typed_runtime.transition.generation;
    typed_meta_unlock();return ok;
}
static bool typed_local(struct nested_parallel_runtime *r,unsigned command,uint64_t epoch){
    unsigned cpu=r->cpu_index;if(cpu>1||!epoch||typed_failed())return false;
    switch(command){
    case TYPED_RESET:return nested_owner_sparse_reset(&r->sparse);
    case TYPED_INVEPT:
        if(typed_runtime.command_root_phase>1)return false;
        for(unsigned i=0;i<typed_runtime.root_count;i++)
            if(typed_runtime.roots[i].historical_mask&(UINT64_C(1)<<cpu)){
                if(!typed_invept(typed_runtime.roots[i].eptp))return false;
                typed_runtime.root_ack[typed_runtime.command_root_phase][i][cpu]=(struct typed_root_phase_ack){
                    typed_runtime.live_ids[i],typed_runtime.plan_freeze,typed_runtime.roots[i].eptp,
                    epoch,cpu,typed_runtime.command_root_phase,true};
            }
        return true;
    case TYPED_TLB:return cache_maintenance_tlb(&typed_runtime.snapshots[cpu]);
    case TYPED_CACHE:return cache_maintenance_cache(&typed_runtime.snapshots[cpu]);
    default:return false;
    }
}
static bool typed_command_both(unsigned command,uint64_t epoch){
    if(typed_runtime.sequence==UINT64_MAX)return false;
    typed_runtime.command=command;typed_runtime.command_epoch=epoch;
    uint64_t seq=typed_runtime.sequence+1;
    __atomic_store_n(&typed_runtime.sequence,seq,__ATOMIC_RELEASE);
    if(!typed_local(&nested_runs[0],command,epoch))return false;
    uint64_t start=typed_clock();
    while(__atomic_load_n(&typed_runtime.ack_sequence,__ATOMIC_ACQUIRE)!=seq){
        if(typed_failed()||typed_clock()-start>UINT64_C(30000000000))return false;
        __asm__ volatile("pause");
    }return !typed_failed();
}
static bool typed_phase(unsigned command,bool publish){
    struct cache_mp_transition *t=&typed_runtime.transition;uint64_t epoch=t->generation;
    enum cache_mp_phase phase=t->phase;
    if(!typed_plan_valid()||(publish&&!cache_mp_transition_publish(t,epoch,phase))||
       !typed_command_both(command,epoch))return false;
    if(command==TYPED_INVEPT){
        unsigned p=typed_runtime.command_root_phase;
        for(unsigned i=0;i<typed_runtime.root_count;i++)for(unsigned cpu=0;cpu<2;cpu++)
            if(typed_runtime.roots[i].historical_mask&(UINT64_C(1)<<cpu)){
                struct typed_root_phase_ack *a=&typed_runtime.root_ack[p][i][cpu];
                if(!a->complete||!typed_identity_equal(a->identity,typed_runtime.live_ids[i])||
                   !typed_freeze_equal(a->freeze,typed_runtime.plan_freeze)||a->eptp!=typed_runtime.roots[i].eptp||
                   a->epoch!=epoch||a->cpu!=cpu||a->phase!=p)return false;
            }
    }
    for(unsigned cpu=0;cpu<2;cpu++)if(!cache_mp_transition_ack(t,cpu,epoch,phase))return false;
    return cache_mp_transition_advance(t,epoch);
}
static bool typed_source_retype(unsigned source,unsigned destination){
    /* The structural EPT12 sources are guest read-only; neither owner can
     * resolve them while its source registration is revoked and gate closed. */
    if(typed_runtime.source_registered)return false;
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
    if(typed_runtime.owners[0].source_session.live||typed_runtime.owners[1].source_session.live||
       !nrc_idle(&typed_catalog,typed_catalog.identity))return false;
#endif
    for(size_t k=0;k<typed_source_count();k++){
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
        uint64_t *leaf=typed_catalog_type_leaves[k];
        uint64_t *pt=leaf?leaf-15:NULL;
#else
        uint64_t *pt=paging_phys_to_virt(domain_memory.backing.pages[18+4*k]);
#endif
        uint64_t old=UINT64_C(0xf000)|0x43|((uint64_t)source<<3);
        if(!pt||__atomic_load_n(&pt[15],__ATOMIC_ACQUIRE)!=old)return false;
    }
    for(size_t k=0;k<typed_source_count();k++){
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
        uint64_t *leaf=typed_catalog_type_leaves[k];
        uint64_t *pt=leaf?leaf-15:NULL;
#else
        uint64_t *pt=paging_phys_to_virt(domain_memory.backing.pages[18+4*k]);
#endif
        __atomic_store_n(&pt[15],UINT64_C(0xf000)|0x43|((uint64_t)destination<<3),__ATOMIC_RELEASE);
    }return true;
}
static bool typed_domain_complete(void){
    for(unsigned cpu=0;cpu<2;cpu++)if(!nested_domain_dispatch_maintenance_ack(&domain_dispatch,cpu,typed_runtime.domain_ticket))return false;
    return nested_domain_dispatch_maintenance_publish(&domain_dispatch,typed_runtime.domain_ticket)&&
        nested_domain_dispatch_maintenance_complete(&domain_dispatch,typed_runtime.domain_ticket);
}
static bool typed_transition(unsigned round){
    unsigned source=round?TRL_UC:TRL_WB,destination=round?TRL_WB:TRL_UC;
    if(!typed_runtime.initialized||typed_runtime.lease.type!=source||
       !nested_domain_dispatch_maintenance_begin(&domain_dispatch,NESTED_DOMAIN_TYPED_TRANSITION,&typed_runtime.domain_ticket)||
#ifndef VMX_NESTED_OWNER_OVERLAP_TEST
       domain_dispatch.domain.maintenance_targets!=3||
#endif
       typed_failed())return false;
    if(!typed_command_both(TYPED_RESET,typed_runtime.lease.type_epoch)||!typed_sources(false)||!typed_meta_lock())return false;
    bool ok=trl_close(&typed_runtime.lease)&&trl_seal(&typed_runtime.lease,&typed_runtime.freeze);
    typed_meta_unlock();
    if(!ok||!typed_collect(true)||!typed_scan(source)||
       !cache_maintenance_lease_current(&typed_runtime.snapshots[0],source,&typed_runtime.hostlease))return false;
    struct cache_page_lease *host=&typed_runtime.hostlease,*initial=&typed_runtime.initial_host;
    if(host->root_hpa!=initial->root_hpa||host->target_hpa!=initial->target_hpa||host->table_hpa!=initial->table_hpa||
       host->index!=initial->index||((host->oldraw^initial->oldraw)&~UINT64_C(0xf8)))return false;
    uint64_t epoch;
    if(!cache_mp_transition_begin(&typed_runtime.transition,typed_runtime.lease.type_epoch,
        destination==TRL_WB?CACHE_MP_OPEN_WB:CACHE_MP_OPEN_UC,true,true,&epoch))return false;
    typed_runtime.plan_epoch=epoch;typed_runtime.plan_freeze=typed_runtime.freeze;
    typed_runtime.plan=(struct domain_typed_alias_plan){0};
    for(unsigned p=0;p<2;p++)for(unsigned i=0;i<TYPED_DYNAMIC_ROOTS;i++)for(unsigned cpu=0;cpu<2;cpu++)
        typed_runtime.root_ack[p][i][cpu]=(struct typed_root_phase_ack){0};
    if(!domain_typed_alias_prepare(&typed_runtime.plan,&typed_runtime.request,source,destination,epoch,
        typed_table,0,&typed_runtime.workspace)||!typed_plan_valid()||
       !domain_typed_alias_detach(&typed_runtime.plan,epoch,&typed_runtime.workspace))return false;
    typed_runtime.command_root_phase=0;
    if(!typed_phase(TYPED_INVEPT,true))return false;
    uint64_t *pte=paging_phys_to_virt(host->table_hpa);if(!pte)return false;
    pte+=host->index;uint64_t old=__atomic_load_n(pte,__ATOMIC_ACQUIRE);
    if(!cache_page_lease_matches(host,old))return false;
    __atomic_store_n(pte,0,__ATOMIC_RELEASE);
    if(!typed_phase(TYPED_TLB,true)||!typed_phase(TYPED_CACHE,false))return false;
    /* No target data access occurs anywhere between break and make. */
    if(__atomic_load_n(pte,__ATOMIC_ACQUIRE)!=0)return false;
    __atomic_store_n(pte,(old&~UINT64_C(0x98))|(destination==TRL_WB?0:0x18),__ATOMIC_RELEASE);
    if(!typed_phase(TYPED_TLB,true)||!typed_phase(TYPED_CACHE,false)||
       !typed_source_retype(source,destination)||!typed_plan_valid()||
       !domain_typed_alias_publish(&typed_runtime.plan,epoch,&typed_runtime.workspace))return false;
    typed_runtime.command_root_phase=1;
    if(!typed_phase(TYPED_INVEPT,true)||!cache_mp_transition_finish(&typed_runtime.transition,epoch)||
       !typed_scan(destination)||!cache_maintenance_lease_current(&typed_runtime.snapshots[0],destination,&typed_runtime.hostlease)||
       !typed_meta_lock())return false;
    ok=typed_runtime.transition_serial!=UINT64_MAX&&trl_freeze_valid(&typed_runtime.lease,typed_runtime.freeze);
    for(struct trl_binding *b=typed_runtime.lease.bindings;b&&ok;b=b->next)if(b->token_live){
        struct trl_token prepared;
        ok=trl_prepare_token(&typed_runtime.lease,typed_runtime.freeze,b,b->token,epoch,destination,&prepared);
    }
    /* The six completed hardware phases above, not refcounts, provide these
     * attestations. Root phases separately checked every historical CPU. */
    struct trl_transition_evidence evidence={.freeze=typed_runtime.freeze,.new_epoch=epoch,
        .serial=ok?++typed_runtime.transition_serial:0,.closed=3,.drained=3,.tlb_before=3,.cache_before=3,
        .tlb_after=3,.cache_after=3,.destination=destination,.complete_alias_audit=true,
        .all_refills_revoked=true,.all_roots_invalidated=true,.host_break_before_make=true,
        .all_aliases_retyped=true,.owner_stamps_ready=true};
    if(ok)ok=trl_reopen(&typed_runtime.lease,typed_runtime.freeze,&evidence);
    typed_meta_unlock();
    return ok&&typed_sources(true)&&typed_domain_complete(); /* Physical consumer gate opens last. */
}
static bool typed_initialize(void){
    if(!domain_memory.typed_dynamic_prepared||!cache_maintenance_same(&typed_runtime.snapshots[0],&typed_runtime.snapshots[1])||
       typed_runtime.snapshots[0].apic!=0||typed_runtime.snapshots[1].apic!=1||
       !cache_maintenance_lease_current(&typed_runtime.snapshots[0],TRL_WB,&typed_runtime.hostlease)||
       !typed_collect(false)||typed_runtime.root_count!=7||!typed_scan(TRL_WB))return false;
    typed_runtime.initial_host=typed_runtime.hostlease;
    typed_runtime.initialized=true;
    return nested_parallel_trace_text(&nested_runs[0],"NESTED.N38C_DYNAMIC_READY roots=7 budget=2 keys=3 epoch=1\n");
}
static bool typed_terminal(void){
    if(typed_runtime.lease.type!=TRL_WB||typed_runtime.lease.type_epoch!=3||
       !nested_domain_dispatch_maintenance_begin(&domain_dispatch,NESTED_DOMAIN_TYPED_OBSERVE,&typed_runtime.domain_ticket)||
#ifndef VMX_NESTED_OWNER_OVERLAP_TEST
       domain_dispatch.domain.maintenance_targets!=3||
#endif
       !typed_sources(false)||!typed_collect(false)||
       !typed_scan(TRL_WB)||!cache_maintenance_lease_current(&typed_runtime.snapshots[0],TRL_WB,&typed_runtime.hostlease))return false;
    typed_runtime.terminal_audit=true;
    return typed_domain_complete();
}
static bool typed_checkpoint(struct nested_parallel_runtime *r,unsigned phase){
    if(!r||r->cpu_index>1||phase>3)return false;
    unsigned cpu=r->cpu_index;
    if(__atomic_load_n(&typed_runtime.released[phase],__ATOMIC_ACQUIRE)||
       (phase==0&&!__atomic_load_n(&typed_runtime.released[2],__ATOMIC_ACQUIRE))||
       (phase==1&&!__atomic_load_n(&typed_runtime.released[0],__ATOMIC_ACQUIRE))||
       (phase==3&&!__atomic_load_n(&typed_runtime.released[1],__ATOMIC_ACQUIRE)))goto fail;
    if(phase==2&&!cache_maintenance_precheck(domain_memory.backing.pages[14],&typed_runtime.snapshots[cpu]))goto fail;
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    if(!typed_overlap_checkpoint(r,phase))goto fail;
#endif
    if(!nested_parallel_trace_text(r,"NESTED.TYPED_CHECKPOINT=")||!nested_parallel_trace_hex(r,phase))goto fail;
    __atomic_store_n(&typed_runtime.stopped[phase][cpu],1,__ATOMIC_RELEASE);
    /* Owner 0 finishes each guest round before owner 1 can enter. No guest
     * mutual barrier can deadlock this deliberately serial ownership test. */
#ifndef VMX_NESTED_OWNER_OVERLAP_TEST
    if(!cpu&&phase!=2)__atomic_store_n(&typed_runtime.turn,1,__ATOMIC_RELEASE);
#endif
    if(cpu){
        uint64_t last=__atomic_load_n(&typed_runtime.ack_sequence,__ATOMIC_ACQUIRE),start=typed_clock();
        while(!__atomic_load_n(&typed_runtime.released[phase],__ATOMIC_ACQUIRE)){
            if(typed_failed()||typed_clock()-start>UINT64_C(30000000000))goto fail;
            uint64_t seq=__atomic_load_n(&typed_runtime.sequence,__ATOMIC_ACQUIRE);
            if(seq!=last){
                if(seq!=last+1||!typed_local(r,typed_runtime.command,typed_runtime.command_epoch))goto fail;
                last=seq;__atomic_store_n(&typed_runtime.ack_sequence,seq,__ATOMIC_RELEASE);start=typed_clock();
            }
            __asm__ volatile("pause");
        }
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
        /* Legacy fixed-root retirement is still version-fenced. Stop AP
         * HERE, before final L1/CLEAR/sparse_destroy, not only owner_retire. */
        if(phase==3&&!typed_overlap_wait(&typed_runtime.teardown_turn))goto fail;
#endif
        return !typed_failed();
    }
    if(!typed_wait32(&typed_runtime.stopped[phase][1])||
       !(phase==2?typed_initialize():phase==3?typed_terminal():typed_transition(phase)))goto fail;
    if(!nested_parallel_trace_text(r,"NESTED.N38C_PHASE epoch=")||!nested_parallel_trace_hex(r,typed_runtime.lease.type_epoch)||
       !nested_parallel_trace_text(r," type=")||!nested_parallel_trace_hex(r,typed_runtime.lease.type))goto fail;
#ifndef VMX_NESTED_OWNER_OVERLAP_TEST
    __atomic_store_n(&typed_runtime.turn,0,__ATOMIC_RELEASE);
#endif
    __atomic_store_n(&typed_runtime.released[phase],1,__ATOMIC_RELEASE);return true;
fail:typed_fatal(r);return false;
}
static bool typed_context_before_entry(struct nested_parallel_runtime *r,bool in_l2){
    if(!r||r->cpu_index>1||!typed_runtime.initialized)return false;
    unsigned cpu=r->cpu_index;
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    if(!typed_overlap_entry(r)){typed_fatal(r);return false;}
#else
    uint64_t start=typed_clock();
    while(__atomic_load_n(&typed_runtime.turn,__ATOMIC_ACQUIRE)!=cpu){
        if(typed_failed()||typed_clock()-start>UINT64_C(30000000000)){typed_fatal(r);return false;}
        __asm__ volatile("pause");
    }
#endif
    if(!typed_guard(r)||
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
       (in_l2&&!nested_owner_catalog_entry(&typed_runtime.owners[cpu]))||
#endif
       !typed_meta_lock()){typed_fatal(r);return false;}
    struct trl_binding *binding=&typed_runtime.fixed_bindings[cpu];
    bool ok=!typed_failed()&&!typed_runtime.bridges[cpu].directory.faulted&&
        cache_mp_transition_entry_allowed(&typed_runtime.transition,typed_runtime.lease.type_epoch);
    if(in_l2){
        struct nested_ept_context_item *item=r->sparse.hardware_item;struct tr_registry_slot *slot=NULL;
        for(struct tr_registry_slot *s=typed_runtime.bridges[cpu].slots;s;s=s->next)
            if(item&&(&s->directory==item->transaction_token)){slot=s;break;}
        ok=ok&&typed_runtime.source_registered&&typed_runtime.source_epoch==typed_runtime.lease.type_epoch&&
            item&&item==r->sparse.contexts.active&&item->pins&&slot&&
            slot->directory.item==item&&!slot->directory.forgotten&&
            slot->directory.record.cache_generation==item->tree.generation;
                if(ok)binding=&slot->typed;
    }
    struct td_operation op={0};uint64_t n=0;struct trl_token token;
    if(ok)ok=trl_token_read(&typed_runtime.lease,binding,&token)&&td_begin(&typed_runtime.directory,&op,&n);
    if(ok)ok=trl_admit(&typed_runtime.lease,&op,n,binding,token,typed_runtime.lease.type,true,false)&&
        td_resident(&typed_runtime.directory,&op,n,binding->record,token.identity,cpu);
    if(op.live){bool ended=td_end(&typed_runtime.directory,&op,n);ok=ok&&ended;}
    typed_meta_unlock();if(!ok)typed_fatal(r);return ok;
}
/* Fixed roots receive durable typed receipts while VMX is still on. Each
 * exact owner ticket survives the unlocked actual INVEPT, never a later
 * invented post-OFF flush. Serial teardown prevents global-version races. */
static bool typed_fixed_retire(unsigned i,unsigned cpu){
    if(!typed_meta_lock())return false;
    struct td_record *r=&typed_runtime.fixed_records[i];struct trl_binding *b=&typed_runtime.fixed_bindings[i];
    struct td_operation op={0};uint64_t n=0,version=0;struct td_retire_ticket ticket={0};struct trl_token token;
    bool ok=r->identity.owner==cpu&&!(r->history&~(UINT64_C(1)<<cpu))&&
        trl_token_read(&typed_runtime.lease,b,&token)&&td_begin(&typed_runtime.directory,&op,&n)&&
        td_retire_begin(&typed_runtime.directory,&op,n,r,r->identity,&ticket);
    if(ok)version=typed_runtime.directory.version;
    typed_meta_unlock();if(!ok)return false;
    if(!typed_invept(ticket.full_eptp))return false;
    if(!typed_meta_lock())return false;
    ok=typed_runtime.directory.version==version&&typed_runtime.flush_serial!=UINT64_MAX&&
        td_retire_commit(&typed_runtime.directory,&op,n,r,r->identity,ticket,ticket.history,
            ++typed_runtime.flush_serial,&typed_runtime.fixed_receipts[i]);
    uint64_t receipt_version=typed_runtime.directory.version;
    if(ok)ok=trl_retire(&typed_runtime.lease,&op,n,b,token,ticket,&typed_runtime.fixed_receipts[i],receipt_version)&&
        td_forget(&typed_runtime.directory,&op,n,r,r->identity);
    if(op.live){bool ended=td_end(&typed_runtime.directory,&op,n);ok=ok&&ended;}
    typed_meta_unlock();return ok;
}
static bool typed_owner_retire(struct nested_parallel_runtime *r){
    if(!r||r->cpu_index>1||typed_failed()||!typed_runtime.terminal_audit||!r->sparse.destroyed||
       !r->vcpu.cleared01||!r->vcpu.cleared02||r->sparse.pool.live||
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
       __atomic_load_n(&typed_runtime.teardown_turn,__ATOMIC_ACQUIRE)!=r->cpu_index)return false;
#else
       __atomic_load_n(&typed_runtime.turn,__ATOMIC_ACQUIRE)!=r->cpu_index)return false;
#endif
    unsigned cpu=r->cpu_index;
    for(unsigned i=0;i<TYPED_DYNAMIC_FIXED;i++)if(typed_runtime.fixed_ids[i].owner==cpu)
        if(!typed_fixed_retire(i,cpu)){typed_fatal(r);return false;}
    if(!cache_maintenance_cache(&typed_runtime.snapshots[cpu])){typed_fatal(r);return false;}
    __atomic_store_n(&typed_runtime.final_cache[cpu],1,__ATOMIC_RELEASE);
    __atomic_store_n(&typed_runtime.retired[cpu],1,__ATOMIC_RELEASE);
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    if(!cpu)__atomic_store_n(&typed_runtime.teardown_turn,1,__ATOMIC_RELEASE);
#else
    if(!cpu)__atomic_store_n(&typed_runtime.turn,1,__ATOMIC_RELEASE);
#endif
    return true;
}
/* Caller has verified both local VMXOFFs, actual CLEARs and AP parked. This
 * final receipt-only snapshot performs no INVEPT and never reads old HPAs. */
static bool typed_final(void){
    if(!typed_runtime.initialized||typed_failed()||!typed_runtime.terminal_audit||
       !__atomic_load_n(&typed_runtime.retired[0],__ATOMIC_ACQUIRE)||!__atomic_load_n(&typed_runtime.retired[1],__ATOMIC_ACQUIRE)||
       !__atomic_load_n(&typed_runtime.final_cache[0],__ATOMIC_ACQUIRE)||!__atomic_load_n(&typed_runtime.final_cache[1],__ATOMIC_ACQUIRE)||
       !nested_parallel_done(&nested_runs[0])||!nested_parallel_done(&nested_runs[1])||
       !cache_maintenance_lease_current(&typed_runtime.snapshots[0],TRL_WB,&typed_runtime.hostlease)||!typed_meta_lock())return false;
    bool ok=!typed_runtime.directory.records&&!typed_runtime.directory.operations&&!typed_runtime.source_registered&&
        typed_runtime.lease.type==TRL_WB&&typed_runtime.lease.type_epoch==3&&!typed_runtime.lease.live_tokens&&
        !typed_runtime.bridges[0].directory.faulted&&!typed_runtime.bridges[1].directory.faulted&&
        trl_close(&typed_runtime.lease)&&trl_seal(&typed_runtime.lease,&typed_runtime.freeze)&&
        trl_snapshot_read(&typed_runtime.lease,typed_runtime.freeze,typed_runtime.snapshot,
            TYPED_DYNAMIC_SNAPSHOT,&typed_runtime.snapshot_count);
#ifdef VMX_NESTED_OWNER_ROOT_SCOPED_TEST
    for(unsigned cpu=0;ok&&cpu<2;cpu++){
        unsigned count=0;
        for(struct tr_registry_slot *s=typed_runtime.bridges[cpu].slots;s&&ok;s=s->next){
            ok=tr_local_completed_matches(&typed_runtime.local_provider[cpu],s);
            if(ok)count++;
        }
        ok=ok&&count==typed_runtime.slots_used[cpu]&&count==nested_runs[cpu].sparse.creations;
        typed_runtime.provider_receipts[cpu]=count;
    }
#endif
#ifdef VMX_NESTED_OWNER_OVERLAP_TEST
    if(ok)ok=typed_overlap_final();
#endif
    typed_runtime.receipt_count=0;
    for(size_t n=0;ok&&n<typed_runtime.snapshot_count;n++){
        struct trl_snapshot *s=&typed_runtime.snapshot[n];
        ok=!s->scan_allowed&&s->root.receipt&&s->root.state==TD_RETIRED&&s->receipt_version&&s->receipt_type_epoch;
        if(ok)typed_runtime.receipt_count++;
    }
    struct trl_final_evidence evidence={.freeze=typed_runtime.freeze,.serial=typed_runtime.flush_serial,
        .parked=3,.vmcs_gone=3,.refills_revoked=3,.roots_flushed=3,.caches_flushed=3,
        .restored_wb=true,.host_wb_verified=true,.complete_alias_audit=typed_runtime.terminal_audit};
    if(ok)ok=typed_runtime.receipt_count==typed_runtime.lease.reserved&&
        trl_backing_release_ready(&typed_runtime.lease,typed_runtime.freeze,&evidence);
    typed_meta_unlock();return ok;
}
static bool typed_contexts_ready(struct nested_owner_sparse *s){
#ifdef VMX_NESTED_ROOT_CATALOG_TEST
    /* This diagnostic fixture deliberately maps its structural pages RO at
     * the same L2 address; this is not a reusable catalog admission rule. */
    if(!s||!nested_owner_catalog_probe(s->dynamic_typed,0x10000,0x40000))return false;
#endif
    unsigned cpu=s->pool.owner;if(cpu>1||s->contexts.count!=1||!s->evict||!s->contexts.active||
       s->contexts.active->key!=0x1001e||!typed_meta_lock())return false;
    struct typed_root_registry *bridge=&typed_runtime.bridges[cpu];
    struct tr_registry_slot *slot=bridge->slots;
    bool ok=typed_runtime.slots_used[cpu]==1&&!bridge->directory.faulted&&slot&&!slot->next&&
        slot->directory.item==s->contexts.active&&slot->directory.record.state==TD_LIVE&&
        slot->typed.token_live&&slot->typed.token.type==TRL_WB&&slot->typed.token.type_epoch==1;
    typed_meta_unlock();return ok;
}
static bool typed_configure(struct nested_parallel_runtime *r){
    if(!r||r->cpu_index>1||!domain_memory.typed_dynamic_prepared)return false;
    unsigned cpu=r->cpu_index;
    if(!typed_runtime.configured){
        if(cpu||!td_init(&typed_runtime.directory,domain_dispatch.domain.id,domain_dispatch.domain.incarnation,3,1)||
           !trl_init(&typed_runtime.lease,&typed_runtime.directory,
               (struct trl_backing){domain_memory.backing.pages[14],TYPED_DYNAMIC_BACKING_LIFETIME,true},
               2*TYPED_DYNAMIC_SLOTS+TYPED_DYNAMIC_FIXED)||
           !cache_mp_transition_init(&typed_runtime.transition,3)||!typed_fixed_init())return false;
        typed_runtime.configured=true;
    }
#ifdef VMX_NESTED_OWNER_ROOT_SCOPED_TEST
    const struct tr_registry_root_provider provider={typed_provider_reserve,typed_provider_root_receipt};
    if(!typed_root_registry_init_root_scoped(&typed_runtime.bridges[cpu],&typed_runtime.lease,cpu,
        domain_memory.backing.pages[14],&provider,r))return false;
    typed_runtime.local_provider[cpu]=(struct tr_local_provider){.bridge=&typed_runtime.bridges[cpu],
        .slots=typed_runtime.slots[cpu],.completed=typed_runtime.completed[cpu],.slot_count=TYPED_DYNAMIC_SLOTS,
        .owner=cpu,.serial=&typed_runtime.flush_serial,.lock=typed_provider_lock,.unlock=typed_bridge_unlock,
        .current_cpu=typed_provider_cpu,.healthy=typed_provider_healthy,.invalidate=typed_provider_invalidate,.context=r};
#else
    const struct tr_registry_provider provider={typed_provider_reserve,typed_provider_receipt};
    if(!typed_root_registry_init(&typed_runtime.bridges[cpu],&typed_runtime.lease,cpu,
        domain_memory.backing.pages[14],&provider,r))return false;
#endif
    if(!typed_root_registry_set_sync(&typed_runtime.bridges[cpu],typed_bridge_lock,typed_bridge_unlock,0))return false;
    typed_runtime.owners[cpu]=(struct nested_owner_dynamic_typed){.bridge=&typed_runtime.bridges[cpu],
        .environment_guard=typed_guard,.guard_context=r,.fatal=typed_fatal,.fatal_context=r};
    #ifdef VMX_NESTED_ROOT_CATALOG_TEST
    typed_runtime.owners[cpu].catalog=&typed_catalog;
    typed_runtime.owners[cpu].catalog_identity=typed_catalog.identity;
    typed_runtime.owners[cpu].initial_key=typed_catalog.roots[0].full_eptp;
#endif
    r->sparse.multiple=true;r->sparse.evict=true;r->sparse.dynamic_typed=&typed_runtime.owners[cpu];
    r->sparse.typed_transactions=typed_root_registry_transactions();
    r->sparse.typed_transaction_context=&typed_runtime.bridges[cpu];
    r->sparse.contexts_ready=typed_contexts_ready;return true;
}
