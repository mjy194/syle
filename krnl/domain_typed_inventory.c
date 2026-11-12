#include "domain_typed_inventory.h"
#include <stddef.h>
static uint64_t digest(uint64_t hash,uint64_t value) {
    for(unsigned byte=0;byte<8;byte++) {
        hash^=(value>>(byte*8))&255;
        hash*=UINT64_C(1099511628211);
    }
    return hash;
}
static bool page(uint64_t hpa,uint64_t mask){return !(hpa&~mask);}
enum domain_typed_status domain_typed_inventory_audit_type(
    const struct domain_typed_request *q,unsigned nonwb_type,domain_typed_table_resolver resolve,void *ctx,
    struct domain_typed_workspace *w,struct domain_typed_inventory *out)
{
    if((nonwb_type!=0&&nonwb_type!=4)||!q||!resolve||!w||!out||(void *)out==(void *)w||!q->roots||
       !q->root_count||q->root_count>DOMAIN_TYPED_ROOTS||q->maxphys<12||
       q->maxphys>48||!q->node_budget)return DOMAIN_TYPED_INVALID;
    uint64_t mask=((UINT64_C(1)<<q->maxphys)-1)&~UINT64_C(4095);
    if(!page(q->target_hpa,mask))return DOMAIN_TYPED_INVALID;
    w->result=(struct domain_typed_inventory){.target_hpa=q->target_hpa,.root_count=q->root_count};
    for(unsigned root=0;root<q->root_count;root++) {
        struct domain_typed_root_binding b=q->roots[root];
        if(!b.id||!b.generation||(b.eptp&4095)!=0x1e||
           (b.eptp&~(mask|UINT64_C(4095))))return DOMAIN_TYPED_INVALID;
        for(unsigned j=0;j<root;j++)
            if(q->roots[j].id==b.id)return DOMAIN_TYPED_INVALID;
        w->result.roots[root].binding=b;
        uint64_t *hash=&w->result.roots[root].scan_digest;
        *hash=UINT64_C(14695981039346656037);
        *hash=digest(*hash,b.id);*hash=digest(*hash,b.generation);
        *hash=digest(*hash,b.eptp);*hash=digest(*hash,b.historical_mask);
        unsigned level=0;
        uint64_t next=b.eptp&mask,prefix=0;
        uint8_t permissions=7;
        bool entering=true;
        for(;;) {
            if(entering) {
                if(next==q->target_hpa)return DOMAIN_TYPED_OWNERSHIP;
                for(unsigned p=0;p<level;p++)if(w->frames[p].hpa==next)return DOMAIN_TYPED_OWNERSHIP;
                if(w->result.total_nodes==q->node_budget)return DOMAIN_TYPED_BUDGET;
                const uint64_t *view;
                if(!resolve(ctx,next,&view)||!view||((uintptr_t)view&4095))return DOMAIN_TYPED_OWNERSHIP;
                /* Distinct HPAs must not be presented as the same ancestor view. */
                for(unsigned p=0;p<level;p++)if(w->frames[p].view==view)return DOMAIN_TYPED_OWNERSHIP;
                w->frames[level]=(struct domain_typed_frame){view,next,prefix,0,permissions};
                *hash=digest(*hash,next);*hash=digest(*hash,prefix);*hash=digest(*hash,level);
                w->result.total_nodes++;w->result.roots[root].visited_nodes++;
                entering=false;
            }
            struct domain_typed_frame *f=&w->frames[level];
            if(f->slot==512) {
                if(!level)break;
                level--;continue;
            }
            unsigned slot=f->slot++;
            uint64_t raw=f->view[slot];
            *hash=digest(*hash,slot);*hash=digest(*hash,raw);
            if(!(raw&7))continue; /* no present path, architecturally ignored */
            if(!(raw&1))return DOMAIN_TYPED_UNSUPPORTED;
            uint64_t allowed=mask|7|(level==3?UINT64_C(0x78):0);
            if(raw&~allowed)return DOMAIN_TYPED_UNSUPPORTED;
            uint64_t gpa=f->gpa_prefix|((uint64_t)slot<<(39-level*9));
            uint8_t effective=f->permissions&(raw&7);
            if(level<3) {
                next=raw&mask;prefix=gpa;permissions=effective;level++;entering=true;continue;
            }
            uint64_t hpa=raw&mask;
            unsigned mt=(raw>>3)&7;
            if(hpa!=q->target_hpa) {
                if(mt!=6)return DOMAIN_TYPED_UNSUPPORTED;
                continue;
            }
            if((mt!=nonwb_type&&mt!=6)||!(raw&0x40)||(raw&4))return DOMAIN_TYPED_UNSUPPORTED;
            if(w->result.alias_count==DOMAIN_TYPED_ALIASES)return DOMAIN_TYPED_CAPACITY;
            w->result.aliases[w->result.alias_count++]=(struct domain_typed_alias){
                root,gpa,hpa,raw,effective,(uint8_t)mt,true};
            w->result.roots[root].aliases++;
        }
    }
    *out=w->result;
    return DOMAIN_TYPED_OK;
}

enum domain_typed_status domain_typed_inventory_audit(
    const struct domain_typed_request *q,domain_typed_table_resolver resolve,void *ctx,
    struct domain_typed_workspace *w,struct domain_typed_inventory *out) {
    return domain_typed_inventory_audit_type(q,0,resolve,ctx,w,out);
}
