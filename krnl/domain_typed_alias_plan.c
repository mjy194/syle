#include "domain_typed_alias_plan.h"
#include <stddef.h>
static bool failure(struct domain_typed_alias_plan *p){if(p)p->phase=DTA_FAILED;return false;}
static bool binding_equal(struct domain_typed_root_binding a,struct domain_typed_root_binding b) {
    return a.id==b.id&&a.generation==b.generation&&a.eptp==b.eptp&&a.historical_mask==b.historical_mask;
}
static bool inventory_equal(const struct domain_typed_inventory *a,const struct domain_typed_inventory *b) {
    if(a->target_hpa!=b->target_hpa||a->total_nodes!=b->total_nodes||
       a->root_count!=b->root_count||a->alias_count!=b->alias_count)return false;
    for(unsigned i=0;i<a->root_count;i++)
        if(!binding_equal(a->roots[i].binding,b->roots[i].binding)||
           a->roots[i].visited_nodes!=b->roots[i].visited_nodes||
           a->roots[i].scan_digest!=b->roots[i].scan_digest||
           a->roots[i].aliases!=b->roots[i].aliases)return false;
    for(unsigned i=0;i<a->alias_count;i++){
        const struct domain_typed_alias *x=&a->aliases[i],*y=&b->aliases[i];
        if(x->root_index!=y->root_index||x->gpa!=y->gpa||x->hpa!=y->hpa||
           x->raw!=y->raw||x->permissions!=y->permissions||
           x->memory_type!=y->memory_type||x->ignore_pat!=y->ignore_pat)return false;
    }
    return true;
}
static bool locate(struct domain_typed_alias_plan *p,unsigned i,bool zero) {
    const struct domain_typed_alias *a=&p->before.aliases[i];
    uint64_t mask=((UINT64_C(1)<<p->request.maxphys)-1)&~UINT64_C(4095);
    uint64_t hpa=p->roots[a->root_index].eptp&mask;
    for(unsigned level=0;level<4;level++){
        const uint64_t *view;
        if(!p->resolve(p->context,hpa,&view)||!view||((uintptr_t)view&4095))return false;
        unsigned slot=(a->gpa>>(39-level*9))&511;
        uint64_t raw=view[slot];
        if(level==3){
            if(raw!=(zero?0:a->raw))return false;
            if(p->phase==DTA_EMPTY)
                p->slots[i]=(struct domain_typed_alias_slot){(uint64_t *)(uintptr_t)&view[slot],hpa,raw,slot};
            else if(p->slots[i].pointer!=&view[slot]||p->slots[i].table_hpa!=hpa||
                    p->slots[i].index!=slot||p->slots[i].raw!=a->raw)return false;
            return true;
        }
        if(!(raw&1))return false;
        hpa=raw&mask;
    }
    return false;
}
struct overlay_context {struct domain_typed_alias_plan *plan;struct domain_typed_alias_workspace *work;};
static bool overlay_resolve(void *context,uint64_t hpa,const uint64_t **view) {
    struct overlay_context *c=context;
    for(unsigned i=0;i<c->work->overlay_count;i++)
        if(c->work->overlay_hpa[i]==hpa){*view=c->work->overlay[i];return true;}
    return c->plan->resolve(c->plan->context,hpa,view);
}
bool domain_typed_alias_prepare_type(struct domain_typed_alias_plan *p,
    const struct domain_typed_request *q,unsigned source,unsigned dest,uint64_t epoch,unsigned nonwb_type,
    domain_typed_table_resolver resolve,void *context,struct domain_typed_alias_workspace *w) {
    if((nonwb_type!=0&&nonwb_type!=4)||!p||!q||!q->roots||!w||!resolve||!epoch||p->phase!=DTA_EMPTY||
       !q->root_count||q->root_count>DOMAIN_TYPED_ROOTS||
       !((source==6&&dest==nonwb_type)||(source==nonwb_type&&dest==6)))return failure(p);
    p->request=*q;
    for(unsigned i=0;i<q->root_count;i++)p->roots[i]=q->roots[i];
    p->request.roots=p->roots;p->resolve=resolve;p->context=context;
    p->nonwb_type=nonwb_type;p->epoch=epoch;p->source_type=source;p->destination_type=dest;
    if(domain_typed_inventory_audit_type(&p->request,p->nonwb_type,resolve,context,&w->scan,&p->before)!=DOMAIN_TYPED_OK)
        return failure(p);
    if(!p->before.alias_count)return failure(p);
    w->overlay_count=0;
    for(unsigned i=0;i<p->before.alias_count;i++){
        if(p->before.aliases[i].memory_type!=source||!locate(p,i,false))return failure(p);
        struct domain_typed_alias_slot *s=&p->slots[i];
        unsigned n=0;while(n<w->overlay_count&&w->overlay_hpa[n]!=s->table_hpa)n++;
        if(n==w->overlay_count){
            const uint64_t *view;
            if(!resolve(context,s->table_hpa,&view)||!view)return failure(p);
            w->overlay_hpa[n]=s->table_hpa;
            for(unsigned j=0;j<512;j++)w->overlay[n][j]=view[j];
            w->overlay_count++;
        }
        w->overlay[n][s->index]=0;
    }
    /* Compute expected detached state without modifying live tables. */
    struct overlay_context overlay={p,w};
    if(domain_typed_inventory_audit_type(&p->request,p->nonwb_type,overlay_resolve,&overlay,&w->scan,&p->detached)!=DOMAIN_TYPED_OK||
       p->detached.alias_count)return failure(p);
    p->phase=DTA_PREPARED;return true;
}
bool domain_typed_alias_detach(struct domain_typed_alias_plan *p,uint64_t epoch,
    struct domain_typed_alias_workspace *w) {
    if(!p||!w||p->phase!=DTA_PREPARED||p->epoch!=epoch)return failure(p);
    if(domain_typed_inventory_audit_type(&p->request,p->nonwb_type,p->resolve,p->context,&w->scan,&w->inventory)!=DOMAIN_TYPED_OK||
       !inventory_equal(&p->before,&w->inventory))return failure(p);
    for(unsigned i=0;i<p->before.alias_count;i++)if(!locate(p,i,false))return failure(p);
    for(unsigned i=0;i<p->before.alias_count;i++)*p->slots[i].pointer=0;
    p->phase=DTA_DETACHED;return true;
}
bool domain_typed_alias_publish(struct domain_typed_alias_plan *p,uint64_t epoch,
    struct domain_typed_alias_workspace *w) {
    if(!p||!w||p->phase!=DTA_DETACHED||p->epoch!=epoch)return failure(p);
    if(domain_typed_inventory_audit_type(&p->request,p->nonwb_type,p->resolve,p->context,&w->scan,&w->inventory)!=DOMAIN_TYPED_OK||
       !inventory_equal(&p->detached,&w->inventory))return failure(p);
    for(unsigned i=0;i<p->before.alias_count;i++)if(!locate(p,i,true))return failure(p);
    for(unsigned i=0;i<p->before.alias_count;i++)
        *p->slots[i].pointer=(p->slots[i].raw&~UINT64_C(0x38))|((uint64_t)p->destination_type<<3)|0x40;
    p->phase=DTA_PUBLISHED;return true;
}

bool domain_typed_alias_prepare(struct domain_typed_alias_plan *p,
    const struct domain_typed_request *q,unsigned source,unsigned dest,uint64_t epoch,
    domain_typed_table_resolver resolve,void *ctx,struct domain_typed_alias_workspace *w) {
    return domain_typed_alias_prepare_type(p,q,source,dest,epoch,0,resolve,ctx,w);
}
