#include "../cache_page.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define NX (1ull<<63)
static uint64_t pages[4][512] __attribute__((aligned(4096)));
static bool unavailable;
static bool readtable(void *ctx,uint64_t hpa,const uint64_t **p)
{
    (void)ctx;
    if(unavailable || hpa<0x1000 || hpa>0x4000 || (hpa&4095))return false;
    *p=pages[hpa/4096-1];return true;
}
static void init(void)
{
    memset(pages,0,sizeof pages);unavailable=false;
    pages[0][0]=0x2003;pages[1][0]=0x3003;pages[2][0]=0x4003;pages[3][8]=0x8003|NX;
}
static enum cache_page_audit_status audit(struct cache_page_lease *l,unsigned budget)
{return cache_page_audit(0x1000,0x8000,6,budget,readtable,0,l);}
static void bad(enum cache_page_audit_status expected,unsigned budget)
{
    struct cache_page_lease l,old;memset(&l,0xa5,sizeof l);memcpy(&old,&l,sizeof l);
    assert(audit(&l,budget)==expected);assert(!memcmp(&l,&old,sizeof l));
}
#define REJECT(call) do {struct cache_page_registry old=r;assert(!(call));assert(!memcmp(&r,&old,sizeof r));}while(0)
static void current_type_test(void) {
    struct cache_page_lease lease,saved;
    init();pages[3][8]|=24; /* UC PAT3 target */
    assert(cache_page_audit_current(0x1000,0x8000,6,0,8,readtable,0,&lease)==CACHE_PAGE_AUDIT_OK);
    assert(lease.oldraw==(NX|0x801b)&&lease.table_visits==4);
    assert(audit(&lease,8)==CACHE_PAGE_ATTRIBUTES); /* legacy remains WB-only */
    for(unsigned scenario=0;scenario<7;scenario++) {
        init();pages[3][8]|=24;
        unsigned type=0,budget=8;uint64_t pat=6;
        enum cache_page_audit_status expected=CACHE_PAGE_ATTRIBUTES;
        if(scenario==0){pages[3][9]=pages[3][8];expected=CACHE_PAGE_ALIAS;}
        if(scenario==1){pages[0][1]=0x2003;expected=CACHE_PAGE_ALIAS;}
        if(scenario==2){pages[2][1]=0x8003;expected=CACHE_PAGE_TABLE_ALIAS;}
        if(scenario==3){pages[3][8]&=~NX;}
        if(scenario==4){type=4;expected=CACHE_PAGE_BAD_INPUT;}
        if(scenario==5){pat|=6ull<<24;expected=CACHE_PAGE_BAD_INPUT;}
        if(scenario==6){budget=3;expected=CACHE_PAGE_BUDGET;}
        memset(&lease,0xa5,sizeof lease);saved=lease;
        assert(cache_page_audit_current(0x1000,0x8000,pat,type,budget,readtable,0,&lease)==expected);
        assert(!memcmp(&lease,&saved,sizeof lease));
    }
    init();assert(cache_page_audit_current(0x1000,0x8000,6,6,8,readtable,0,&lease)==CACHE_PAGE_AUDIT_OK);
    assert(cache_page_audit_current(0x1000,0x8000,6,0,8,readtable,0,&lease)==CACHE_PAGE_ATTRIBUTES);
}
static void wt_current_test(void){
 const uint64_t wb=UINT64_C(0x0606060606060606);
 for(unsigned slot=1;slot<8;slot++){
  uint64_t pat=(wb&~(UINT64_C(255)<<(slot*8)))|(UINT64_C(4)<<(slot*8));
  uint64_t bits=((uint64_t)(slot&1)<<3)|((uint64_t)(slot&2)<<3)|((uint64_t)(slot&4)<<5);
  struct cache_page_lease l,unchanged;
  init();assert(cache_page_audit_current_type(0x1000,0x8000,pat,6,slot,bits,8,readtable,0,&l)==CACHE_PAGE_AUDIT_OK);
  pages[3][8]|=bits;
  assert(cache_page_audit_current_type(0x1000,0x8000,pat,4,slot,bits,8,readtable,0,&l)==CACHE_PAGE_AUDIT_OK);
  assert((l.oldraw&0x98)==bits);
  for(unsigned scenario=0;scenario<8;scenario++){
   init();pages[3][8]|=bits;unsigned selected=slot,type=4;uint64_t encoded=bits,currentpat=pat;
   enum cache_page_audit_status want=CACHE_PAGE_BAD_INPUT;
   if(scenario==0){selected^=1;}
   if(scenario==1){encoded^=8;}
   if(scenario==2){type=0;}
   if(scenario==3){currentpat=wb;}
   if(scenario==4){currentpat=(pat&~UINT64_C(255))|4;}
   if(scenario==5){currentpat=(pat&~(UINT64_C(255)<<56))|(UINT64_C(2)<<56);}
   if(scenario==6){pages[3][8]^=8;want=CACHE_PAGE_ATTRIBUTES;}
   if(scenario==7){pages[3][9]=pages[3][8];want=CACHE_PAGE_ALIAS;}
   memset(&l,0xa5,sizeof l);unchanged=l;
   assert(cache_page_audit_current_type(0x1000,0x8000,currentpat,type,selected,encoded,8,readtable,0,&l)==want);
   assert(!memcmp(&l,&unchanged,sizeof l));
  }
 }
 /* Two WT slots bind the selected lowest slot, not any equal-type alias. */
 init();pages[3][8]|=128;
 assert(cache_page_audit_current_type(0x1000,0x8000,UINT64_C(0x0606060406060406),4,4,128,8,readtable,0,&(struct cache_page_lease){0})==CACHE_PAGE_BAD_INPUT);
}
int main(void)
{
    current_type_test();
    wt_current_test();
    uint64_t need=(UINT64_C(1)<<6)|(UINT64_C(1)<<14)|(UINT64_C(1)<<20)|(UINT64_C(1)<<25);
    assert(cache_uc_vmx_capable(UINT64_C(1)<<63,UINT64_C(1)<<33,need));
    assert(!cache_uc_vmx_capable(0,UINT64_C(1)<<33,need));
    assert(!cache_uc_vmx_capable(UINT64_C(1)<<63,0,need));
    for(unsigned bit=0;bit<64;bit++)if(need&(UINT64_C(1)<<bit))
        assert(!cache_uc_vmx_capable(UINT64_C(1)<<63,UINT64_C(1)<<33,need&~(UINT64_C(1)<<bit)));

    struct cache_page_lease l;
    init();assert(audit(&l,4)==CACHE_PAGE_AUDIT_OK);
    assert(l.table_hpa==0x4000&&l.index==8&&l.oldraw==(0x8003|NX)&&l.table_visits==4&&l.audited);
    assert(cache_page_lease_matches(&l,l.oldraw|0x60));assert(!cache_page_lease_matches(&l,l.oldraw^0x1000));
    assert(!cache_page_lease_matches(&l,l.oldraw^2));assert(!cache_page_lease_matches(&l,l.oldraw^8));
    init();bad(CACHE_PAGE_BUDGET,3);
    init();unavailable=true;bad(CACHE_PAGE_UNREADABLE,4);
    init();pages[3][9]=pages[3][8];bad(CACHE_PAGE_ALIAS,4);
    init();pages[0][256]=pages[0][0];bad(CACHE_PAGE_ALIAS,8); /* high canonical alias, same tables */
    init();pages[3][9]=pages[3][8];pages[3][8]=0;bad(CACHE_PAGE_ALIAS,4);
    init();pages[3][8]=0;bad(CACHE_PAGE_NOT_MAPPED,4);
    init();pages[3][8]&=~NX;pages[0][0]|=NX;bad(CACHE_PAGE_ATTRIBUTES,4); /* leaf NX mandatory */
    init();pages[3][8]&=~2ull;bad(CACHE_PAGE_ATTRIBUTES,4);
    init();pages[1][0]&=~2ull;bad(CACHE_PAGE_ATTRIBUTES,4);
    for(unsigned bit=3;bit<=7;bit+=4){init();pages[3][8]|=1ull<<bit;bad(CACHE_PAGE_ATTRIBUTES,4);}
    init();pages[2][0]|=8;bad(CACHE_PAGE_ATTRIBUTES,4);
    init();pages[3][8]|=1ull<<48;bad(CACHE_PAGE_UNSUPPORTED,4);
    init();pages[0][0]|=128;bad(CACHE_PAGE_UNSUPPORTED,4);
    init();pages[2][0]=0x83|NX;bad(CACHE_PAGE_HUGE_ALIAS,4);
    init();pages[1][0]=0x83|NX;bad(CACHE_PAGE_HUGE_ALIAS,4);
    init();pages[2][0]=0x1083|NX;bad(CACHE_PAGE_HUGE_ALIAS,4); /* huge PAT12 */
    init();pages[2][0]=0x2083|NX;bad(CACHE_PAGE_UNSUPPORTED,4);
    init();pages[2][1]=0x200083|NX;assert(audit(&l,4)==CACHE_PAGE_AUDIT_OK); /* unrelated huge */
    init();pages[2][1]=0x201083|NX;assert(audit(&l,4)==CACHE_PAGE_AUDIT_OK); /* unrelated huge PAT */
    init();pages[1][1]=0x1003;bad(CACHE_PAGE_CYCLE,8);
    init();pages[1][1]=0x8003;bad(CACHE_PAGE_TABLE_ALIAS,8);
    init();pages[3][20]=UINT64_MAX&~1ull;assert(audit(&l,4)==CACHE_PAGE_AUDIT_OK);
    assert(cache_page_audit(0x1000,0x8000,0,4,readtable,0,&l)==CACHE_PAGE_BAD_INPUT);
    assert(cache_page_audit(0x1000,0x8000,6|(6ull<<24),4,readtable,0,&l)==CACHE_PAGE_BAD_INPUT);
    assert(cache_page_audit(0x1000,1ull<<32,6,4,readtable,0,&l)==CACHE_PAGE_BAD_INPUT);
    assert(cache_page_audit(0x1000,0x8001,6,4,readtable,0,&l)==CACHE_PAGE_BAD_INPUT);
    init();assert(audit(&l,4)==CACHE_PAGE_AUDIT_OK);
    struct cache_page_proof proof={true,true,true,true,true};
    struct cache_page_registry r={0};uint64_t ticket=0;
    assert(cache_page_init(&r,&l,&proof));
    for(unsigned n=0;n<3;n++){
        enum cache_page_phase dest=r.phase==CACHE_PAGE_WB?CACHE_PAGE_UC:CACHE_PAGE_WB;
        REJECT(cache_page_begin(&r,r.phase,&proof,&ticket));
        proof.no_dma=false;REJECT(cache_page_begin(&r,dest,&proof,&ticket));proof.no_dma=true;
        assert(cache_page_begin(&r,dest,&proof,&ticket));
        REJECT(cache_page_begin(&r,dest,&proof,&ticket));REJECT(cache_page_commit(&r,ticket));
        for(unsigned step=0;step<CACHE_PAGE_STEPS;step++){
            REJECT(cache_page_ack(&r,ticket-1,(enum cache_page_step)step));
            REJECT(cache_page_ack(&r,ticket,(enum cache_page_step)(step+1)));
            assert(cache_page_ack(&r,ticket,(enum cache_page_step)step));
            REJECT(cache_page_ack(&r,ticket,(enum cache_page_step)step));
        }
        assert(cache_page_commit(&r,ticket));assert(r.phase==dest);
    }
    r.generation=UINT64_MAX;REJECT(cache_page_begin(&r,CACHE_PAGE_WB,&proof,&ticket));
    for(unsigned phase=0;phase<3;phase++){
        assert(cache_page_init(&r,&l,&proof));
        if(phase){assert(cache_page_begin(&r,CACHE_PAGE_UC,&proof,&ticket));if(phase==2)assert(cache_page_ack(&r,ticket,CACHE_PAGE_DETACH_GUEST_INVEPT));}
        cache_page_fail(&r);assert(r.phase==CACHE_PAGE_POISON);
        REJECT(cache_page_begin(&r,CACHE_PAGE_WB,&proof,&ticket));REJECT(cache_page_commit(&r,ticket));
        REJECT(cache_page_ack(&r,ticket,CACHE_PAGE_INSTALL));
    }
    /* WT is additive; existing phase values and UC audit remain unchanged. */
    assert(CACHE_PAGE_WB==0&&CACHE_PAGE_TRANSITION==1&&CACHE_PAGE_UC==2&&CACHE_PAGE_POISON==3);
    assert(cache_page_init(&r,&l,&proof));
    assert(cache_page_begin(&r,CACHE_PAGE_WT,&proof,&ticket));
    for(unsigned step=0;step<CACHE_PAGE_STEPS;step++)assert(cache_page_ack(&r,ticket,(enum cache_page_step)step));
    assert(cache_page_commit(&r,ticket)&&r.phase==CACHE_PAGE_WT);
    REJECT(cache_page_begin(&r,CACHE_PAGE_WT,&proof,&ticket));
    REJECT(cache_page_begin(&r,CACHE_PAGE_UC,&proof,&ticket));
    assert(cache_page_begin(&r,CACHE_PAGE_WB,&proof,&ticket));
    for(unsigned step=0;step<CACHE_PAGE_STEPS;step++)assert(cache_page_ack(&r,ticket,(enum cache_page_step)step));
    assert(cache_page_commit(&r,ticket)&&r.phase==CACHE_PAGE_WB);
    assert(cache_page_begin(&r,CACHE_PAGE_UC,&proof,&ticket));
    for(unsigned step=0;step<CACHE_PAGE_STEPS;step++)assert(cache_page_ack(&r,ticket,(enum cache_page_step)step));
    assert(cache_page_commit(&r,ticket));
    REJECT(cache_page_begin(&r,CACHE_PAGE_WT,&proof,&ticket));
    unsigned slot=99;uint64_t bits=0xabcdef;
    const uint64_t allwb=UINT64_C(0x0606060606060606);
    assert(!cache_page_pat_slot(allwb,4,&slot,&bits)&&slot==99&&bits==0xabcdef);
    for(unsigned i=0;i<8;i++) {
        uint64_t pat=(allwb&~(UINT64_C(255)<<(i*8)))|(UINT64_C(4)<<(i*8));
        slot=99;bits=0xabcdef;
        if(!i){assert(!cache_page_pat_slot(pat,4,&slot,&bits)&&slot==99&&bits==0xabcdef);continue;}
        assert(cache_page_pat_slot(pat,4,&slot,&bits)&&slot==i);
        assert(bits==(((uint64_t)(i&1)<<3)|((uint64_t)(i&2)<<3)|((uint64_t)(i&4)<<5)));
        assert(!(bits&~UINT64_C(0x98)));
        pat&=~(UINT64_C(255)<<(i*8)); /* same position, now UC */
        assert(cache_page_pat_slot(pat,0,&slot,&bits)&&slot==i);
    }
    for(unsigned i=0;i<8;i++)for(unsigned value=0;value<256;value++) {
        if(value==0||value==1||value==4||value==5||value==6||value==7)continue;
        uint64_t pat=(allwb&~(UINT64_C(255)<<(i*8)))|((uint64_t)value<<(i*8));
        /* Include an earlier matching WT slot: later illegal bytes must still reject. */
        if(i!=1)pat=(pat&~(UINT64_C(255)<<8))|(UINT64_C(4)<<8);
        slot=99;bits=0xabcdef;
        assert(!cache_page_pat_slot(pat,4,&slot,&bits)&&slot==99&&bits==0xabcdef);
    }
    const uint64_t repeatedwt=UINT64_C(0x0404040404040406);
    assert(cache_page_pat_slot(repeatedwt,4,&slot,&bits)&&slot==1&&bits==8);
    for(unsigned value=0;value<256;value++)if(value!=0&&value!=4) {
        slot=99;bits=0xabcdef;
        assert(!cache_page_pat_slot(repeatedwt,value,&slot,&bits)&&slot==99&&bits==0xabcdef);
    }
    assert(!cache_page_pat_slot(repeatedwt,4,0,&bits));
    assert(!cache_page_pat_slot(repeatedwt,4,&slot,0));
    puts("cache_page audit and sequence: PASS");
}
