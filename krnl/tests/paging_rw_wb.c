/* Host-only composition test: execute the real seal/split and permission walk.
 * Page-table storage is allocated low host memory. Synthetic frame numbers are
 * never dereferenced, and no privileged instruction or guest is executed. */
#define _GNU_SOURCE
#define PAGING_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../paging.c"
char __kernel_start[1], __text_end[1], __rodata_start[1], __rodata_end[1], __kernel_end[1];
static uint64_t observed_cr3, observed_cr4, observed_pat;
static void *allocations[128];
static unsigned allocated, flushes, checks;
int paging_test_context(void) { return 1; }
void paging_test_flush(void) { ++flushes; }
uint64_t paging_test_cr3(void) { return observed_cr3; }
uint64_t paging_test_cr4(void) { return observed_cr4; }
uint64_t paging_test_pat(void) { return observed_pat; }
uint64_t pmm_address_limit(void) { return UINT64_C(1)<<47; }
uint64_t pmm_alloc_page_below(uint64_t limit) {
    assert(allocated<sizeof(allocations)/sizeof(allocations[0]));
    void *p=mmap(0,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    assert(p!=MAP_FAILED && (uintptr_t)p<limit);
    allocations[allocated++]=p;return (uintptr_t)p;
}
void panic(const char *s) { fprintf(stderr,"panic: %s\n",s);abort(); }
void log_text(const char *s) { (void)s; }
void log_hex(UINT64 n) { (void)n; }
static void reset(void) {
    for(unsigned i=0;i<allocated;++i)assert(!munmap(allocations[i],4096));
    allocated=0;tables=0;flushes=0;ap_start_attempted=0;
    root=new_table();observed_cr3=root;observed_cr4=0;observed_pat=6;
}
static void expect(uint64_t physical,int result) {
    ++checks;
    if(paging_page_rw_wb(physical)!=result) {
        fprintf(stderr,"check %u: frame=%#llx expected=%d\n",checks,
                (unsigned long long)physical,result);abort();
    }
}
static uint64_t va(uint64_t physical) { return (uintptr_t)paging_phys_to_virt(physical); }
static void split_composition(unsigned shift,uint64_t base) {
    reset();
    uint64_t target=base+8192,neighbor=target+4096;
    *entry(va(base),1,shift)=base|NX|131;
    expect(target,1);expect(neighbor,1);
    unsigned before=tables;
    assert(paging_seal_code_page(target));
    assert(flushes==1 && tables==before+(shift-12)/9);
    expect(target,0);expect(neighbor,1);
    /* The second 2 MiB subrange stays huge when splitting a 1 GiB leaf. */
    if(shift==30)expect(base+0x400000,1);
    assert((*leaf(va(target),0)&(NX|7))==(NX|1));
    assert((*leaf(va(neighbor),0)&(NX|7))==(NX|3));
    /* A second seal remains one-way and does not allocate another table. */
    before=tables;
    assert(paging_seal_code_page(target));
    assert(flushes==2 && tables==before);
    expect(target,0);expect(neighbor,1);
}
static void permission_matrix(void) {
    reset();
    uint64_t p=0x802000;
    *leaf(p,1)=p|3|NX;
    uint64_t *path[4];
    for(unsigned level=0;level<4;++level)path[level]=entry(p,0,39-9*level);
    /* U/S is ANDed across the path: only all-user is user-accessible. */
    for(unsigned user_mask=0;user_mask<16;++user_mask) {
        for(unsigned level=0;level<4;++level)
            *path[level]=(*path[level]&~UINT64_C(4))|((user_mask>>level&1)?4:0);
        expect(p,user_mask!=15);
    }
    for(unsigned level=0;level<4;++level)*path[level]&=~UINT64_C(4);
    /* Present and write permission must hold at EVERY level. Keep the
     * existing conservative policy rejecting cache bits on table accesses. */
    for(unsigned level=0;level<4;++level) {
        uint64_t old=*path[level];
        *path[level]=old&~UINT64_C(1);expect(p,0);
        *path[level]=old&~UINT64_C(2);expect(p,0);
        *path[level]=old|8;expect(p,0);
        *path[level]=old|16;expect(p,0);
        *path[level]=old;
    }
    uint64_t original=*path[3];
    *path[3]=original|128;expect(p,0); /* 4 KiB PAT, not huge */
    *path[3]=(p+4096)|3;expect(p,0); /* well-formed wrong translation */
    *path[3]=original&~NX;expect(p,1); /* NX is irrelevant to RW/WB */
    *path[3]=original;
    observed_pat=0;expect(p,0);
    observed_pat=UINT64_C(0x0606060606060600);expect(p,0);
    observed_pat=UINT64_C(0x0102030405060706);expect(p,1);
    observed_cr3=root+4096;expect(p,0);observed_cr3=root;
    observed_cr4=UINT64_C(1)<<12;expect(p,0);
    observed_cr4=UINT64_C(1)<<17;expect(p,0);observed_cr4=0;
    expect(p,1);
}
static void huge_negatives(unsigned shift) {
    reset();
    uint64_t base=UINT64_C(0x140000000),p=base+0x2000;
    uint64_t *e=entry(va(base),1,shift);
    uint64_t good=base|NX|131;
    *e=good;expect(p,1);
    *e=good|4096;expect(p,0); /* large-page PAT */
    *e=good|8192;expect(p,0); /* reserved low address bit */
    *e=good&~UINT64_C(2);expect(p,0);
    *e=good|8;expect(p,0);
    *e=good|16;expect(p,0);
    *e=(base+(UINT64_C(1)<<shift))|NX|131;expect(p,0);
    *e=good|4;
    /* All-user huge leaf rejected; supervisor ancestor still constrains it. */
    for(int level=39;level>(int)shift;level-=9)*entry(va(base),0,level)|=4;
    expect(p,0);
    *entry(va(base),0,39)&=~UINT64_C(4);expect(p,1);
}
static void pat_split(unsigned shift) {
    reset();
    uint64_t base=UINT64_C(0x140000000),p=base+8192;
    *entry(va(base),1,shift)=base|NX|4096|131;
    expect(p,0);expect(p+4096,0);
    assert(paging_seal_code_page(p));
    expect(p,0);expect(p+4096,0);
    assert((*leaf(va(p),0)&(NX|128|7))==(NX|128|1));
    assert((*leaf(va(p+4096),0)&(NX|128|7))==(NX|128|3));
}
static void rejected_split(unsigned shift,uint64_t flags) {
    reset();
    uint64_t base=UINT64_C(0x140000000),p=base+8192;
    *entry(va(base),1,shift)=base|NX|128|flags;
    /* An all-user mapping must stay user, rather than becoming accepted
     * merely because it was split. RO and non-WB cases stay rejected too. */
    if(flags&4)
        for(int level=39;level>(int)shift;level-=9)*entry(va(base),0,level)|=4;
    expect(p,0);expect(p+4096,0);
    assert(paging_seal_code_page(p));assert(flushes==1);
    expect(p,0);expect(p+4096,0);
}
static void small_alias_composition(void) {
    reset();
    uint64_t p=0x802000,alias=UINT64_C(0xffff900000002000);
    *leaf(p,1)=p|3|NX;
    *leaf(alias,1)=p|3;
    *leaf(p+4096,1)=(p+4096)|3|NX;
    expect(p,1);expect(p+4096,1);
    unsigned before=tables;
    assert(paging_seal_code_page(p));assert(tables==before&&flushes==1);
    expect(p,0);expect(p+4096,1);
    assert((*leaf(alias,0)&(ADDR|3))==(p|1));
}
int main(void) {
    /* Regression first: pre-fix walker rejects the still-RW neighbor because
     * the real splitter installs an intermediate U/S=1 table pointer. */
    split_composition(21,UINT64_C(0x100000000));
    split_composition(30,UINT64_C(0x140000000));
    split_composition(21,UINT64_C(0x800000));
    permission_matrix();huge_negatives(21);huge_negatives(30);
    pat_split(21);pat_split(30);small_alias_composition();
    for(unsigned shift=21;shift<=30;shift+=9) {
        rejected_split(shift,7);rejected_split(shift,1);
        rejected_split(shift,11);rejected_split(shift,19);
    }
    printf("PASS paging RW/WB: %u production-walker checks; real seal/split composition, effective U/S, RW/PAT/cache/translation/context\n",checks);
    for(unsigned i=0;i<allocated;++i)assert(!munmap(allocations[i],4096));
}
