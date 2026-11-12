/* Read-only host CR3 enumeration only. This serial fixture makes no VMX,
 * concurrent-paging, MTRR, cache-transition, or hardware validation claim. */
#include "../../krnl/cache_page.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define NX (UINT64_C(1)<<63)
#define ALL_WB UINT64_C(0x0606060606060606)
enum { OWNERS=2, TABLES=7, RETAINED=83, FIRST_INDEX=256 };
static uint64_t tables[OWNERS][TABLES][512] __attribute__((aligned(4096)));
static uint64_t retained[RETAINED][512];
static uint64_t saved_tables[OWNERS][TABLES][512],saved_retained[RETAINED][512];
struct resolver { unsigned owner,reads,seen;uint64_t denied;bool unaligned; };

static uint64_t table_hpa(unsigned owner,unsigned page)
{return UINT64_C(0x1000)+(uint64_t)owner*0x10000+(uint64_t)page*4096;}
static uint64_t target_hpa(unsigned page)
{return UINT64_C(0x100000)+(uint64_t)page*4096;}
static bool read_table(void *ctx,uint64_t hpa,const uint64_t **view)
{
    struct resolver *r=ctx;
    ++r->reads;
    if(hpa==r->denied)return false;
    for(unsigned p=0;p<TABLES;p++)if(hpa==table_hpa(r->owner,p)){
        r->seen|=1u<<p;
        *view=tables[r->owner][p]+(r->unaligned?1:0);
        return true;
    }
    return false;
}
static void setup(void)
{
    memset(tables,0,sizeof tables);
    for(unsigned o=0;o<OWNERS;o++){
        tables[o][0][0]=table_hpa(o,1)|3;
        tables[o][1][0]=table_hpa(o,2)|3;
        tables[o][2][0]=table_hpa(o,3)|3;
        for(unsigned p=0;p<RETAINED;p++)tables[o][3][FIRST_INDEX+p]=target_hpa(p)|3|NX;
        /* Unrelated VA path after the target, so success requires continuing
         * the entire root, rather than just resolving the identity address. */
        tables[o][0][1]=table_hpa(o,4)|3;
        tables[o][4][0]=table_hpa(o,5)|3;
        tables[o][5][0]=table_hpa(o,6)|3;
        tables[o][6][0]=UINT64_C(0x500003)|NX;
    }
    for(unsigned p=0;p<RETAINED;p++)for(unsigned w=0;w<512;w++)
        retained[p][w]=UINT64_C(0x470b000000000000)|((uint64_t)p<<16)|w;
}
static void check(struct resolver *r,uint64_t target,uint64_t pat,unsigned budget,
                  enum cache_page_audit_status expected)
{
    struct cache_page_lease out,before;
    memset(&out,0xa5,sizeof out);memcpy(&before,&out,sizeof out);
    memcpy(saved_tables,tables,sizeof tables);
    memcpy(saved_retained,retained,sizeof retained);
    r->reads=0;r->seen=0;
    assert(cache_page_audit_wb(table_hpa(r->owner,0),target,pat,budget,read_table,r,&out)==expected);
    assert(!memcmp(saved_tables,tables,sizeof tables));
    assert(!memcmp(saved_retained,retained,sizeof retained));
    if(expected==CACHE_PAGE_AUDIT_OK){
        assert(out.audited&&out.root_hpa==table_hpa(r->owner,0)&&out.target_hpa==target);
        assert(out.table_hpa==table_hpa(r->owner,3)&&out.index==target/4096);
        assert(out.oldraw==tables[r->owner][3][out.index]);
        assert(out.table_visits==r->reads&&r->reads==TABLES&&r->seen==127);
        assert(cache_page_lease_matches(&out,out.oldraw|0x60));
    }else assert(!memcmp(&out,&before,sizeof out));
    if(expected==CACHE_PAGE_BAD_INPUT)assert(!r->reads);
}
static void all_backing_pages(void)
{
    setup();
    for(unsigned o=0;o<OWNERS;o++)for(unsigned p=0;p<RETAINED;p++){
        struct resolver r={.owner=o};
        check(&r,target_hpa(p),ALL_WB,TABLES,CACHE_PAGE_AUDIT_OK);
        check(&r,target_hpa(p),ALL_WB,TABLES-1,CACHE_PAGE_BUDGET);
        assert(r.reads==TABLES-1);
    }
    puts("PASS N47B WB-only all 83 retained pages on two roots, complete seven-table enumeration, immutable backing");
}
static void pat_admission(void)
{
    setup();struct resolver r={0};
    check(&r,table_hpa(0,0),ALL_WB,TABLES,CACHE_PAGE_TABLE_ALIAS);
    /* No UC or WT anywhere, and PAT3 is not UC. */
    check(&r,target_hpa(0),ALL_WB,TABLES,CACHE_PAGE_AUDIT_OK);
    /* Every legal unrelated PAT type is accepted without using that slot. */
    const unsigned legal[]={0,1,4,5,6,7};
    for(unsigned slot=1;slot<8;slot++)for(unsigned i=0;i<sizeof legal/sizeof legal[0];i++){
        uint64_t pat=(ALL_WB&~(UINT64_C(255)<<(slot*8)))|((uint64_t)legal[i]<<(slot*8));
        check(&r,target_hpa(0),pat,TABLES,CACHE_PAGE_AUDIT_OK);
    }
    for(unsigned slot=0;slot<8;slot++)for(unsigned value=0;value<256;value++){
        bool valid=value==0||value==1||value==4||value==5||value==6||value==7;
        if((slot==0&&value==6)||(slot!=0&&valid))continue;
        uint64_t pat=(ALL_WB&~(UINT64_C(255)<<(slot*8)))|((uint64_t)value<<(slot*8));
        check(&r,target_hpa(0),pat,TABLES,CACHE_PAGE_BAD_INPUT);assert(!r.reads);
    }
    struct cache_page_lease out,before;memset(&out,0xa5,sizeof out);memcpy(&before,&out,sizeof out);
    assert(cache_page_audit(table_hpa(0,0),target_hpa(0),ALL_WB,TABLES,read_table,&r,&out)==CACHE_PAGE_BAD_INPUT);
    assert(cache_page_audit_current(table_hpa(0,0),target_hpa(0),ALL_WB,6,TABLES,read_table,&r,&out)==CACHE_PAGE_BAD_INPUT);
    assert(cache_page_audit_current_type(table_hpa(0,0),target_hpa(0),ALL_WB,6,1,8,TABLES,read_table,&r,&out)==CACHE_PAGE_BAD_INPUT);
    assert(!memcmp(&out,&before,sizeof out));
    puts("PASS N47B all-WB and unusual legal PAT accepted; every illegal byte and non-WB PAT0 rejected; legacy UC/WT requirements retained");
}
static void aliases_and_roles(void)
{
    for(unsigned o=0;o<OWNERS;o++)for(unsigned p=0;p<RETAINED;p++){
        struct resolver r={.owner=o};uint64_t target=target_hpa(p);
        setup();tables[o][6][17]=target|3|NX;
        check(&r,target,ALL_WB,16,CACHE_PAGE_ALIAS);
        /* Extra alias through another PAT index, including equal-type WB
         * and genuinely different UC/WT cache aliases, must all reject. */
        const uint64_t pats[]={ALL_WB,UINT64_C(0x0606060606060006),UINT64_C(0x0606060606060406)};
        for(unsigned i=0;i<sizeof pats/sizeof pats[0];i++){
            tables[o][6][17]|=8;check(&r,target,pats[i],16,CACHE_PAGE_ALIAS);
        }
        setup();tables[o][0][256]=table_hpa(o,1)|3;
        check(&r,target,ALL_WB,16,CACHE_PAGE_ALIAS); /* shared subtree, high VA */
        setup();tables[o][5][1]=target|3;
        check(&r,target,ALL_WB,16,CACHE_PAGE_TABLE_ALIAS); /* host paging role after match */
        setup();tables[o][4][1]=target|3;
        check(&r,target,ALL_WB,16,CACHE_PAGE_TABLE_ALIAS);
        setup();tables[o][0][2]=target|3;
        check(&r,target,ALL_WB,16,CACHE_PAGE_TABLE_ALIAS);
        /* A failure on one owner's root must not fabricate an alias on the
         * other. Each actual CR3 still needs its own complete audit. */
        struct resolver other={.owner=1-o};check(&other,target,ALL_WB,TABLES,CACHE_PAGE_AUDIT_OK);
    }
    for(unsigned level=1;level<=2;level++)for(unsigned bits=0;bits<4;bits++){
        setup();struct resolver r={0};
        tables[0][level==1?4:5][1]=UINT64_C(0x83)|NX|(bits&1?24:0)|(bits&2?4096:0);
        check(&r,target_hpa(0),ALL_WB,16,CACHE_PAGE_HUGE_ALIAS);
    }
    puts("PASS N47B extra/different-cache/high-VA aliases, source host table roles, 1GiB/2MiB huge aliases fail closed on each owner");
}
static void attributes_and_full_tree(void)
{
    for(unsigned scenario=0;scenario<14;scenario++){
        setup();struct resolver r={0};enum cache_page_audit_status want=CACHE_PAGE_ATTRIBUTES;
        switch(scenario){
        case 0:tables[0][3][FIRST_INDEX]&=~NX;break;
        case 1:tables[0][3][FIRST_INDEX]&=~UINT64_C(2);break;
        case 2:tables[0][1][0]&=~UINT64_C(2);break;
        case 3:tables[0][3][FIRST_INDEX]|=8;break;
        case 4:tables[0][3][FIRST_INDEX]|=16;break;
        case 5:tables[0][3][FIRST_INDEX]|=128;break;
        case 6:tables[0][2][0]|=8;break;
        case 7:tables[0][1][0]|=16;break;
        case 8:tables[0][3][FIRST_INDEX]&=~NX;tables[0][0][0]|=NX;break; /* leaf NX contract */
        case 9:r.denied=table_hpa(0,6);want=CACHE_PAGE_UNREADABLE;break;
        case 10:tables[0][6][0]|=UINT64_C(1)<<48;want=CACHE_PAGE_UNSUPPORTED;break;
        case 11:tables[0][5][1]=table_hpa(0,0)|3;want=CACHE_PAGE_CYCLE;break;
        case 12:tables[0][3][FIRST_INDEX]=0;want=CACHE_PAGE_NOT_MAPPED;break;
        case 13:r.unaligned=true;want=CACHE_PAGE_UNREADABLE;break;
        }
        check(&r,target_hpa(0),ALL_WB,16,want);
    }
    setup();struct resolver r={0};
    /* Nonpresent fields are ignored, unrelated huge mappings are allowed. */
    tables[0][6][25]=UINT64_MAX&~UINT64_C(1);tables[0][5][1]=UINT64_C(0x201083)|NX;
    check(&r,target_hpa(0),ALL_WB,TABLES,CACHE_PAGE_AUDIT_OK);
    puts("PASS N47B effective RW/NX and PAT0 attributes; late unreadable/unsupported/cyclic paths; unchanged failed outputs and page bytes");
}
static void bad_input(void)
{
    setup();struct resolver r={0};
    check(&r,0,ALL_WB,TABLES,CACHE_PAGE_BAD_INPUT);
    check(&r,target_hpa(0)+1,ALL_WB,TABLES,CACHE_PAGE_BAD_INPUT);
    check(&r,UINT64_C(0x100000000),ALL_WB,TABLES,CACHE_PAGE_BAD_INPUT);
    check(&r,target_hpa(0),ALL_WB,0,CACHE_PAGE_BAD_INPUT);
    struct cache_page_lease out,before;memset(&out,0xa5,sizeof out);memcpy(&before,&out,sizeof out);
    const uint64_t roots[]={0,0x1001,UINT64_C(1)<<48};
    for(unsigned i=0;i<sizeof roots/sizeof roots[0];i++)
        assert(cache_page_audit_wb(roots[i],target_hpa(0),ALL_WB,TABLES,read_table,&r,&out)==CACHE_PAGE_BAD_INPUT);
    assert(cache_page_audit_wb(table_hpa(0,0),target_hpa(0),ALL_WB,TABLES,NULL,&r,&out)==CACHE_PAGE_BAD_INPUT);
    assert(cache_page_audit_wb(table_hpa(0,0),target_hpa(0),ALL_WB,TABLES,read_table,&r,NULL)==CACHE_PAGE_BAD_INPUT);
    assert(!r.reads&&!memcmp(&out,&before,sizeof out));
    puts("PASS N47B invalid roots/targets/budget/callback/output fail without a table read");
}
int main(void)
{
    all_backing_pages();pat_admission();aliases_and_roles();attributes_and_full_tree();bad_input();
    return 0;
}
