#include "cache_page.h"
#include <stddef.h>
#define ADDRESS UINT64_C(0x0000fffffffff000)
#define NX (UINT64_C(1)<<63)
struct audit {
    cache_page_read_fn read;
    void *ctx;
    uint64_t target,root,path[4];
    unsigned visits,budget,matches;
    uint64_t target_pat_bits;
    struct cache_page_lease lease;
};
static enum cache_page_audit_status scan(struct audit *a,uint64_t table,
    unsigned level,uint64_t va,bool writable,bool nx,bool ordinary_path)
{
    if(table==a->target)return CACHE_PAGE_TABLE_ALIAS;
    if(table&~ADDRESS)return CACHE_PAGE_UNSUPPORTED;
    unsigned depth=4-level;
    for(unsigned i=0;i<depth;i++)if(a->path[i]==table)return CACHE_PAGE_CYCLE;
    if(a->visits==a->budget)return CACHE_PAGE_BUDGET;
    ++a->visits;a->path[depth]=table;
    const uint64_t *view=NULL;
    if(!a->read(a->ctx,table,&view) || !view || ((uintptr_t)view&4095))return CACHE_PAGE_UNREADABLE;
    for(unsigned index=0;index<512;index++){
        uint64_t e=view[index];
        if(!(e&1))continue; /* Nonpresent fields are not interpreted. */
        if(e&~(ADDRESS|UINT64_C(0xfff)|NX))return CACHE_PAGE_UNSUPPORTED;
        if(level==4 && (e&128))return CACHE_PAGE_UNSUPPORTED;
        unsigned shift=12+9*(level-1);
        uint64_t next_va=va|((uint64_t)index<<shift);
        bool next_rw=writable && (e&2), next_nx=nx || (e&NX);
        bool large=level>1 && (e&128);
        bool leaf=level==1 || large;
        uint64_t base=e&ADDRESS;
        if(!leaf){
            enum cache_page_audit_status s=scan(a,base,level-1,next_va,next_rw,next_nx,ordinary_path&&!(e&24));
            if(s!=CACHE_PAGE_AUDIT_OK)return s;
            continue;
        }
        uint64_t span=UINT64_C(1)<<shift;
        if(large){
            /* Huge-page PAT bit12 is not an address/alignment violation. */
            base&=~UINT64_C(4096);
            if(base&(span-1))return CACHE_PAGE_UNSUPPORTED;
        }
        if(a->target<base || a->target-base>=span)continue;
        if(large)return CACHE_PAGE_HUGE_ALIAS;
        if(++a->matches!=1)return CACHE_PAGE_ALIAS;
        if(next_va!=a->target)return CACHE_PAGE_ALIAS;
        if(!(e&2) || !(e&NX) || !next_rw || !next_nx || !ordinary_path || (e&(UINT64_C(128)|24))!=a->target_pat_bits)
            return CACHE_PAGE_ATTRIBUTES;
        a->lease=(struct cache_page_lease){.root_hpa=a->root,.target_hpa=a->target,
            .table_hpa=table,.oldraw=e,.index=index,.audited=true};
    }
    return CACHE_PAGE_AUDIT_OK;
}
static enum cache_page_audit_status audit_bits(uint64_t root,uint64_t target,uint64_t bits,
    unsigned budget,cache_page_read_fn read,void *ctx,struct cache_page_lease *out)
{
    if(!out||!read||!budget||!root||(root&~ADDRESS)||!target||(target&4095)||target>=UINT64_C(0x100000000))return CACHE_PAGE_BAD_INPUT;
    struct audit a={.read=read,.ctx=ctx,.target=target,.root=root,.budget=budget,.target_pat_bits=bits};
    enum cache_page_audit_status status=scan(&a,root,4,0,true,false,true);
    if(status!=CACHE_PAGE_AUDIT_OK)return status;
    if(!a.matches)return CACHE_PAGE_NOT_MAPPED;
    a.lease.table_visits=a.visits;*out=a.lease;return CACHE_PAGE_AUDIT_OK;
}
enum cache_page_audit_status cache_page_audit_wb(uint64_t root,uint64_t target,uint64_t pat,
    unsigned budget,cache_page_read_fn read,void *ctx,struct cache_page_lease *out)
{
    if((pat&255)!=6)return CACHE_PAGE_BAD_INPUT;
    for(unsigned i=0;i<8;i++){
        unsigned type=(pat>>(8*i))&255;
        if(type!=0 && type!=1 && type!=4 && type!=5 && type!=6 && type!=7)
            return CACHE_PAGE_BAD_INPUT;
    }
    return audit_bits(root,target,0,budget,read,ctx,out);
}
enum cache_page_audit_status cache_page_audit_current_type(uint64_t root,uint64_t target,uint64_t pat,
    unsigned type,unsigned slot,uint64_t bits,unsigned budget,cache_page_read_fn read,void *ctx,struct cache_page_lease *out)
{
    unsigned actual_slot;uint64_t actual_bits;
    if((type!=4&&type!=6)||!cache_page_pat_slot(pat,4,&actual_slot,&actual_bits)||slot!=actual_slot||bits!=actual_bits)
        return CACHE_PAGE_BAD_INPUT;
    return audit_bits(root,target,type==4?bits:0,budget,read,ctx,out);
}
enum cache_page_audit_status cache_page_audit_current(uint64_t root,uint64_t target,uint64_t pat,
    unsigned current_type,unsigned budget,cache_page_read_fn read,void *ctx,struct cache_page_lease *out)
{
    if((current_type!=0&&current_type!=6) || !out || !read || !budget || !root || (root&~ADDRESS) || !target ||
       (target&4095) || target>=UINT64_C(0x100000000) || (pat&255)!=6 || ((pat>>24)&255)!=0)
        return CACHE_PAGE_BAD_INPUT;
    struct audit a={.read=read,.ctx=ctx,.target=target,.root=root,.budget=budget,.target_pat_bits=current_type==0?24:0};
    enum cache_page_audit_status s=scan(&a,root,4,0,true,false,true);
    if(s!=CACHE_PAGE_AUDIT_OK)return s;
    if(!a.matches)return CACHE_PAGE_NOT_MAPPED;
    a.lease.table_visits=a.visits;*out=a.lease;return CACHE_PAGE_AUDIT_OK;
}
enum cache_page_audit_status cache_page_audit(uint64_t root,uint64_t target,uint64_t pat,
    unsigned budget,cache_page_read_fn read,void *ctx,struct cache_page_lease *out)
{
    return cache_page_audit_current(root,target,pat,6,budget,read,ctx,out);
}
bool cache_page_lease_matches(const struct cache_page_lease *l,uint64_t raw)
{
    return l && l->audited && !((l->oldraw^raw)&~UINT64_C(0x60));
}
bool cache_page_pat_slot(uint64_t pat,uint8_t requested,unsigned *slot,uint64_t *bits)
{
    if(!slot || !bits || (requested!=0 && requested!=4) || (pat&255)!=6)return false;
    for(unsigned i=0;i<8;i++) {
        unsigned type=(pat>>(8*i))&255;
        if(type!=0 && type!=1 && type!=4 && type!=5 && type!=6 && type!=7)return false;
    }
    for(unsigned i=0;i<8;i++)if(((pat>>(8*i))&255)==requested) {
        uint64_t encoded=((uint64_t)(i&1)<<3)|((uint64_t)(i&2)<<3)|((uint64_t)(i&4)<<5);
        *slot=i;*bits=encoded;return true;
    }
    return false;
}
static bool proven(const struct cache_page_proof *p)
{
    return p && p->owned_private_ram && p->single_cpu && p->guest_quiescent &&
        p->no_dma && p->frozen_mappings;
}
bool cache_page_init(struct cache_page_registry *r,const struct cache_page_lease *l,
    const struct cache_page_proof *p)
{
    if(!r || !l || !l->audited || !proven(p) || !l->table_visits || l->index>=512 ||
       !l->root_hpa || (l->root_hpa&~ADDRESS) || (l->table_hpa&~ADDRESS) ||
       !l->target_hpa || l->target_hpa>=UINT64_C(0x100000000) || (l->target_hpa&4095) ||
       (l->oldraw&ADDRESS)!=l->target_hpa || !(l->oldraw&1))return false;
    *r=(struct cache_page_registry){.lease=*l,.phase=CACHE_PAGE_WB,
        .destination=CACHE_PAGE_WB,.generation=1};return true;
}
bool cache_page_begin(struct cache_page_registry *r,enum cache_page_phase dest,
    const struct cache_page_proof *p,uint64_t *ticket)
{
    if(!r || !ticket || !proven(p) || !r->lease.audited || !r->generation ||
       (r->phase!=CACHE_PAGE_WB && r->phase!=CACHE_PAGE_UC && r->phase!=CACHE_PAGE_WT) ||
       (dest!=CACHE_PAGE_WB && dest!=CACHE_PAGE_UC && dest!=CACHE_PAGE_WT) || dest==r->phase ||
       (r->phase!=CACHE_PAGE_WB && dest!=CACHE_PAGE_WB) ||
       r->generation==UINT64_MAX)return false;
    r->destination=dest;r->phase=CACHE_PAGE_TRANSITION;r->next_step=0;
    *ticket=++r->generation;return true;
}
bool cache_page_ack(struct cache_page_registry *r,uint64_t ticket,enum cache_page_step step)
{
    if(!r || r->phase!=CACHE_PAGE_TRANSITION || ticket!=r->generation ||
       (unsigned)step>=CACHE_PAGE_STEPS || (unsigned)step!=r->next_step)return false;
    ++r->next_step;return true;
}
bool cache_page_commit(struct cache_page_registry *r,uint64_t ticket)
{
    if(!r || r->phase!=CACHE_PAGE_TRANSITION || ticket!=r->generation ||
       r->next_step!=CACHE_PAGE_STEPS)return false;
    r->phase=r->destination;return true;
}
void cache_page_fail(struct cache_page_registry *r)
{if(r)r->phase=CACHE_PAGE_POISON;}
