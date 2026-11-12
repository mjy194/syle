#define _GNU_SOURCE
#define PAGING_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../paging.c"
char __kernel_start[1], __text_end[1], __rodata_start[1], __rodata_end[1], __kernel_end[1];
static unsigned flushes;
static int valid_context=1;
int paging_test_context(void) { return valid_context; }
void paging_test_flush(void) { ++flushes; }
uint64_t pmm_address_limit(void) { return UINT64_C(1)<<47; }
uint64_t pmm_alloc_page_below(uint64_t limit) {
    void *p=mmap(0,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    assert(p!=MAP_FAILED && (uintptr_t)p<limit); return (uintptr_t)p;
}
void panic(const char *s) { fprintf(stderr,"panic: %s\n",s); abort(); }
void log_text(const char *s) { (void)s; }
void log_hex(UINT64 n) { (void)n; }
static uint64_t raw(uint64_t va) { return *leaf(va,0); }
int main(void) {
    root=new_table();
    const uint64_t pa=0x802000, direct=DIRECT_BASE+pa, user=0x8000000000;
    *leaf(pa,1)=pa|3|NX;
    *leaf(user,1)=pa|5; /* RX alias: all other aliases must lose W */
    *leaf(direct,1)=pa|3|NX|24;
    *leaf(pa+4096,1)=(pa+4096)|3|NX;
    assert(!paging_seal_code_page(0));
    assert(!paging_seal_code_page(pa+1));
    assert(!paging_seal_code_page(UINT64_C(1)<<47));
    valid_context=0;assert(!paging_seal_code_page(pa));valid_context=1;
    paging_note_ap_start();assert(!paging_seal_code_page(pa));ap_start_attempted=0;
    assert(!paging_seal_code_page(0x900000));assert(flushes==0);
    assert(paging_seal_code_page(pa));assert(flushes==1);
    assert(raw(pa)==(pa|1|NX));assert(raw(user)==(pa|5));
    assert(raw(direct)==(pa|1|NX|24));assert(raw(pa+4096)==((pa+4096)|3|NX));
    assert(paging_seal_code_page(pa));assert(flushes==2);
    /* 2 MiB alias PAT conversion, NX/user/cache preservation and neighbors. */
    uint64_t h=0xffff900000000000ULL;
    *entry(h,1,21)=0x800000|NX|4096|128|7|24;
    assert(paging_seal_code_page(pa));assert(flushes==3);
    assert(raw(h+8192)==(pa|NX|128|5|24));
    assert(raw(h+4096)==(0x801000|NX|128|7|24));
    /* 1 GiB -> 2 MiB -> 4 KiB split, including large PAT flag. */
    uint64_t g=0xffffa00000000000ULL;
    *entry(g,1,30)=NX|4096|128|3;
    assert(paging_seal_code_page(pa));assert(flushes==4);
    assert(raw(g+pa)==(pa|NX|128|1));
    assert(raw(g+pa+4096)==((pa+4096)|NX|128|3));
    assert((*entry(g+0x200000,0,21)&(NX|4096|128|3))==(NX|4096|128|3));
    uint64_t high=UINT64_C(0x100002000);
    *entry(DIRECT_BASE+0x100000000ULL,1,21)=0x100000000ULL|NX|131;
    assert(paging_seal_code_page(high));
    assert(raw(DIRECT_BASE+high)==(high|NX|1));
    assert(raw(DIRECT_BASE+high+4096)==((high+4096)|NX|3));
    /* Address formula: exact low/high boundary and exclusive canonical cap. */
    assert((uintptr_t)paging_phys_to_virt(0xffffffff)==0xffffffff);
    assert((uintptr_t)paging_phys_to_virt(0x100000000)==DIRECT_BASE+0x100000000);
    assert((uintptr_t)paging_phys_to_virt((UINT64_C(1)<<47)-1)==UINT64_MAX);
    assert(!paging_phys_to_virt(UINT64_C(1)<<47));
    puts("PASS paging: all physical aliases sealed, huge split/PAT, rejection, TLB hook, boundaries");
}
