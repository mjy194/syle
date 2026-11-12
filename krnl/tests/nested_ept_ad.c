#include "../nested_ept_ad.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t tables[4][512] __attribute__((aligned(4096)));
struct source {uint64_t leaf_page;int fail,wrong_hpa,readonly,bad_pointer;};
static struct source init(unsigned leaf_level) {
    memset(tables,0,sizeof tables);
    tables[0][0]=0x2007;
    if(leaf_level==3) tables[1][1]=UINT64_C(0x80000000)|0xb7;
    else {
        tables[1][0]=0x3007;
        if(leaf_level==2) tables[2][1]=0x4000b7;
        else {tables[2][0]=0x4007;tables[3][15]=0x8037;}
    }
    return (struct source){.leaf_page=leaf_level==3?0x80003000:leaf_level==2?0x401000:0x8000,
        .fail=-1,.wrong_hpa=-1,.readonly=-1,.bad_pointer=-1};
}
static bool read_page(void *context,uint64_t gpa,uint8_t need,uint64_t *hpa,
    const uint64_t **view,uint8_t *perm) {
    struct source *s=context;
    if(gpa>=0x1000&&gpa<=0x4000) {
        unsigned i=(unsigned)(gpa/4096)-1;assert(need==1);
        *hpa=0x100000+i*4096;*view=tables[i];*perm=7;return true;
    }
    assert(gpa==s->leaf_page);*hpa=0x900000;*view=NULL;*perm=7;return true;
}
static bool write_page(void *context,uint64_t gpa,uint8_t need,uint64_t *hpa,
    uint64_t **view,uint8_t *perm) {
    struct source *s=context;assert(need==3);
    assert(gpa>=0x1000&&gpa<=0x4000&&!(gpa&4095));
    int i=(int)(gpa/4096)-1;
    if(i==s->fail) return false;
    *hpa=i==s->wrong_hpa?0x800000:0x100000+(unsigned)i*4096;
    *view=tables[i]+(i==s->bad_pointer?1:0);*perm=i==s->readonly?1:7;
    return true;
}
static struct nested_ept_result walk(struct source *s,unsigned level,struct nested_ept_ad_trace *t) {
    const struct nested_ept_policy p={48,true,true};
    return nested_ept_ad_walk(0x105e,level==3?0x40003123:level==2?0x201123:0xf123,
        1,read_page,s,&p,t);
}
static uint64_t *entry(const struct nested_ept_ad_entry *e) {
    return &tables[(e->entry_gpa>>12)-1][(e->entry_gpa&4095)/8];
}
static void clear_ad(const struct nested_ept_ad_trace *t) {
    for(unsigned i=0;i<t->count;i++) *entry(&t->entries[i])&=~UINT64_C(0x300);
}
int main(void) {
    struct nested_ept_ad_trace t;
    for(unsigned level=1;level<=3;level++) {
        struct source s=init(level);
        assert(walk(&s,level,&t).status==NESTED_EPT_OK);
        assert(t.count==5-level&&t.leaf_level==level&&t.maxphys==48);
        for(unsigned i=0;i<t.count;i++) {
            assert(t.entries[i].leaf==(i+1==t.count));
            assert(t.entries[i].table_hpa==0x100000+i*4096);
            assert((*entry(&t.entries[i])&0x300)==0); /* walk is read-only */
        }
        assert(nested_ept_ad_update(&t,false,write_page,&s));
        for(unsigned i=0;i<t.count;i++) assert((*entry(&t.entries[i])&0x300)==0x100);
        /* Cached read trace is reusable on an actual later write. */
        assert(nested_ept_ad_update(&t,true,write_page,&s));
        for(unsigned i=0;i<t.count;i++)
            assert((*entry(&t.entries[i])&0x300)==(i+1==t.count?0x300:0x100));
        assert(walk(&s,level,&t).status==NESTED_EPT_OK); /* existing A/D legal */
        clear_ad(&t);
        assert(nested_ept_ad_update(&t,false,write_page,&s)); /* sticky observed D */
        assert((*entry(&t.entries[t.count-1])&0x300)==0x300);
        /* No target writes on any later prevalidation failure. */
        for(unsigned index=0;index<t.count;index++) for(unsigned bad=0;bad<5;bad++) {
            s=init(level);assert(walk(&s,level,&t).status==NESTED_EPT_OK);
            if(bad==0) s.fail=(int)index;
            if(bad==1) s.wrong_hpa=(int)index;
            if(bad==2) s.readonly=(int)index;
            if(bad==3) s.bad_pointer=(int)index;
            if(bad==4) *entry(&t.entries[index])^=0x1000; /* stale mapping identity */
            uint64_t before[4][512];memcpy(before,tables,sizeof before);
            assert(!nested_ept_ad_update(&t,true,write_page,&s));
            assert(memcmp(before,tables,sizeof before)==0);
        }
        s=init(level);tables[0][0]&=~UINT64_C(2);
        assert(walk(&s,level,&t).status==NESTED_EPT_OK);
        uint64_t before[4][512];memcpy(before,tables,sizeof before);
        assert(!nested_ept_ad_update(&t,true,write_page,&s));
        assert(memcmp(before,tables,sizeof before)==0);
        assert(nested_ept_ad_update(&t,false,write_page,&s));

        s=init(level);tables[0][0]|=0x200; /* nonleaf D unsupported */
        t.count=4;assert(walk(&s,level,&t).status==NESTED_EPT_MALFORMED_UNSUPPORTED&&t.count==0);
        s=init(level);tables[0][0]=UINT64_C(0xfffffffffffffff8);
        assert(walk(&s,level,&t).status==NESTED_EPT_L1_DENIED&&t.count==0);
    }
    struct source s=init(1);const struct nested_ept_policy p={48,true,true};
    assert(nested_ept_ad_walk(0x101e,0xf123,1,read_page,&s,&p,&t).status==NESTED_EPT_MALFORMED_UNSUPPORTED&&t.count==0);
    assert(nested_ept_ad_walk(0x105e,0xf123,1,read_page,&s,&p,NULL).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    assert(nested_ept_walk_policy(0x105e,0xf123,1,read_page,&s,&p).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    tables[0][0]|=0x100;
    assert(nested_ept_walk(0x101e,0xf123,1,read_page,&s).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    s=init(1);assert(walk(&s,1,&t).status==NESTED_EPT_OK);
    struct nested_ept_ad_trace saved=t;
    for(unsigned bad=0;bad<8;bad++) {
        t=saved;
        if(bad==0) t.count=5;
        if(bad==1) t.leaf_level=2;
        if(bad==2) t.maxphys=49;
        if(bad==3) t.entries[0].entry_gpa|=1;
        if(bad==4) t.entries[0].table_hpa|=1;
        if(bad==5) t.entries[1].leaf=true;
        if(bad==6) t.entries[0].raw^=0x1000;
        if(bad==7) t.entries[3].raw|=0x80;
        uint64_t before[4][512];memcpy(before,tables,sizeof before);
        assert(!nested_ept_ad_update(&t,false,write_page,&s));
        assert(memcmp(before,tables,sizeof before)==0);
    }
    assert(!nested_ept_ad_update(NULL,false,write_page,&s));
    assert(!nested_ept_ad_update(&saved,false,NULL,&s));
    puts("PASS EPT12 A/D trace, mutable writeback prevalidation and cached dirty update");
}
