#include "nested_ept_atomic.h"
#include "nested_ept_atomic_format.h"
#include <stddef.h>
_Static_assert(__atomic_always_lock_free(8,0),"EPT entries require lock-free 64-bit atomics");
static uint64_t load(const struct nested_ept_atomic_ops *o,void *c,uint64_t *p) {
    return o->load?o->load(c,p):__atomic_load_n(p,__ATOMIC_ACQUIRE);
}
static bool cas(const struct nested_ept_atomic_ops *o,void *c,uint64_t *p,uint64_t a,uint64_t b) {
    return o->cas?o->cas(c,p,a,b):
        __atomic_compare_exchange_n(p,&a,b,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
static struct nested_ept_atomic_result walk(
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned width,
    const struct nested_ept_atomic_ops *o,void *c,struct ad_entry_stamp stamp,
    struct nested_ept_atomic_trace *trace,bool ipat,bool allow_2m,bool allow_1g,nea_typed_leaf_fn typed)
{
    struct nested_ept_atomic_result r={.status=NEA_INVALID,
        .translation={.status=NESTED_EPT_MALFORMED_UNSUPPORTED}};
    if(trace)*trace=(struct nested_ept_atomic_trace){0};
    if(!trace||!o||!o->guard||!o->table||!o->leaf||!!o->load!=!!o->cas||
       width<12||width>48||!need||(need&~7)||!stamp.domain_id||
       !stamp.incarnation||!stamp.context_id||!stamp.epoch||!stamp.lifetime)return r;
    trace->stamp=stamp;
    uint64_t mask=((UINT64_C(1)<<width)-1)&~UINT64_C(4095);
    if((eptp&4095)!=0x5e || (eptp&~(mask|UINT64_C(4095))) ||
       gpa>=(UINT64_C(1)<<width))return r;
    uint64_t table=eptp&mask;
    uint8_t effective=7;
    bool denied=false;uint8_t leaf_type=6;
    for(unsigned level=0;level<4;level++) {
        if(!o->guard(c,stamp)){r.status=NEA_STALE;return r;}
        uint64_t hpa,*page;
        if(!o->table(c,table,&hpa,&page)||!page||((uintptr_t)page&4095)||
           (hpa&~mask)){r.status=NEA_BACKING;return r;}
        unsigned slot=(unsigned)((gpa>>(39-level*9))&511);
        uint64_t *p=&page[slot],raw=load(o,c,p);
        if(!o->guard(c,stamp)){r.status=NEA_STALE;return r;}
        if(!(raw&7)){denied=true;break;}
        unsigned shift;
        if(!nea_decode_policy(raw,level,mask,ipat,allow_2m,allow_1g,typed!=NULL,&shift)){r.status=NEA_UNSUPPORTED;return r;}
        bool leaf=shift!=0;
        trace->entry[trace->count++]=(struct nested_ept_atomic_entry){
            .pointer=p,.entry_gpa=table+slot*8,.table_hpa=hpa,.raw=raw,
            .leaf=leaf,.depth=level,.leaf_shift=shift};
        effective&=raw&7;
        table=leaf?nea_leaf_page(raw,gpa,mask,shift):raw&mask;
        if(leaf){leaf_type=(raw>>3)&7;break;}
    }
    r.translation.effective12=effective;
    if(denied) {
        r.translation.status=NESTED_EPT_L1_DENIED;
        r.translation.effective12=0;
    }else{
        uint64_t hpa;uint8_t permissions,type=6;
        if(!o->guard(c,stamp)){r.status=NEA_STALE;return r;}
        if(!(typed?typed(c,table,&hpa,&permissions,&type):o->leaf(c,table,&hpa,&permissions))||(hpa&~mask)||
           (permissions&~7)||!(permissions&1)||type!=leaf_type||(typed&&type==0&&(permissions&4))){
            r.status=NEA_BACKING;return r;
        }
        if(!o->guard(c,stamp)){r.status=NEA_STALE;return r;}
        r.translation=(struct nested_ept_result){
            .status=(permissions&need)==need?NESTED_EPT_OK:NESTED_EPT_L0_BACKING_DENIED,
            .l1_gpa=table+(gpa&4095),.hpa=hpa+(gpa&4095),
            .effective12=effective,.effective02=effective&permissions,.leaf_resolved=true,
            .memory_explicit=ipat,.ignore_pat=ipat,.leaf_type=ipat?leaf_type:0,.walk_type=ipat?6:0};
        if(r.translation.status!=NESTED_EPT_OK){r.status=NEA_BACKING;return r;}
        if((effective&need)!=need)r.translation.status=NESTED_EPT_L1_DENIED;
    }
    for(unsigned i=0;i<trace->count;i++) {
        const struct nested_ept_atomic_entry *e=&trace->entry[i];
        uint64_t verify_hpa,*verify_page;
        if(!o->guard(c,stamp)){r.status=NEA_STALE;return r;}
        if(!o->table(c,e->entry_gpa&~UINT64_C(4095),&verify_hpa,&verify_page)||
           verify_hpa!=e->table_hpa || !verify_page ||
           ((uintptr_t)verify_page&4095) ||
           &verify_page[(e->entry_gpa&4095)/8]!=e->pointer) {
            r.status=NEA_BACKING;return r;
        }
        if(!o->guard(c,stamp)){r.status=NEA_STALE;return r;}
        uint64_t mutable=0x100|(e->leaf?0x200:0);
        uint64_t bits=0x100|((e->leaf && r.translation.status==NESTED_EPT_OK && (need&2))?0x200:0);
        bool done=false;
        for(unsigned attempt=0;attempt<64;attempt++) {
            if(!o->guard(c,stamp)){r.status=NEA_STALE;return r;}
            uint64_t now=load(o,c,e->pointer);
            if(!o->guard(c,stamp)){r.status=NEA_STALE;return r;}
            if((now&~mutable)!=(e->raw&~mutable)){r.status=NEA_REWALK;return r;}
            if((now&bits)==bits){done=true;break;}
            if(cas(o,c,e->pointer,now,now|bits)){
                r.changed=true;
                if(!o->guard(c,stamp)){r.status=NEA_STALE;return r;}
                done=true;break;
            }
            if(!o->guard(c,stamp)){r.status=NEA_STALE;return r;}
        }
        if(!done){r.status=NEA_RETRY;return r;}
    }
    if(!o->guard(c,stamp)){r.status=NEA_STALE;return r;}
    r.status=NEA_DONE;
    return r;
}

struct nested_ept_atomic_result nested_ept_atomic_walk(
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned width,
    const struct nested_ept_atomic_ops *ops,void *context,struct ad_entry_stamp stamp,
    struct nested_ept_atomic_trace *trace) {
    return walk(eptp,gpa,need,width,ops,context,stamp,trace,false,false,false,NULL);
}
struct nested_ept_atomic_result nested_ept_atomic_walk_wb_ipat(
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned width,
    const struct nested_ept_atomic_ops *ops,void *context,struct ad_entry_stamp stamp,
    struct nested_ept_atomic_trace *trace) {
    struct nested_ept_atomic_result r=walk(eptp,gpa,need,width,ops,context,stamp,trace,true,false,false,NULL);
    if(r.status==NEA_BACKING)r.translation.status=NESTED_EPT_L0_BACKING_DENIED;
    else if(r.translation.status==NESTED_EPT_MALFORMED_UNSUPPORTED)
        r.translation.status=NESTED_EPT_UNSUPPORTED;
    return r;
}

struct nested_ept_atomic_result nested_ept_atomic_walk_wb_ipat_2m(
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned width,
    const struct nested_ept_atomic_ops *ops,void *context,struct ad_entry_stamp stamp,
    struct nested_ept_atomic_trace *trace) {
    struct nested_ept_atomic_result r=walk(eptp,gpa,need,width,ops,context,stamp,trace,true,true,false,NULL);
    if(r.status==NEA_BACKING)r.translation.status=NESTED_EPT_L0_BACKING_DENIED;
    else if(r.translation.status==NESTED_EPT_MALFORMED_UNSUPPORTED)
        r.translation.status=NESTED_EPT_UNSUPPORTED;
    return r;
}

struct nested_ept_atomic_result nested_ept_atomic_walk_wb_ipat_1g(
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned width,
    const struct nested_ept_atomic_ops *ops,void *context,struct ad_entry_stamp stamp,
    struct nested_ept_atomic_trace *trace) {
    struct nested_ept_atomic_result r=walk(eptp,gpa,need,width,ops,context,stamp,trace,true,true,true,NULL);
    if(r.status==NEA_BACKING)r.translation.status=NESTED_EPT_L0_BACKING_DENIED;
    else if(r.translation.status==NESTED_EPT_MALFORMED_UNSUPPORTED)
        r.translation.status=NESTED_EPT_UNSUPPORTED;
    return r;
}

struct nested_ept_atomic_result nested_ept_atomic_walk_uc_ipat(
    uint64_t eptp,uint64_t gpa,uint8_t need,unsigned width,
    const struct nested_ept_atomic_ops *ops,nea_typed_leaf_fn typed,void *context,
    struct ad_entry_stamp stamp,struct nested_ept_atomic_trace *trace) {
    if(!typed)return (struct nested_ept_atomic_result){.status=NEA_INVALID,
        .translation={.status=NESTED_EPT_UNSUPPORTED}};
    struct nested_ept_atomic_result r=walk(eptp,gpa,need,width,ops,context,stamp,trace,true,false,false,typed);
    if(r.status==NEA_BACKING)r.translation.status=NESTED_EPT_L0_BACKING_DENIED;
    else if(r.translation.status==NESTED_EPT_MALFORMED_UNSUPPORTED)
        r.translation.status=NESTED_EPT_UNSUPPORTED;
    return r;
}
