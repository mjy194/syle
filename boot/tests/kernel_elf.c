#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel_elf.h"
#include "../kernel_prepare.h"
struct eh {
    unsigned char ident[16]; uint16_t type,machine; uint32_t version;
    uint64_t entry,phoff,shoff; uint32_t flags;
    uint16_t ehsize,phentsize,phnum,shentsize,shnum,shstrndx;
};
struct ph { uint32_t type,flags; uint64_t offset,vaddr,paddr,filesz,memsz,align; };
static unsigned char image[0x3000], output[0x5000];
static void valid(void) {
    memset(image,0,sizeof image);
    struct eh h = {.ident={0x7f,'E','L','F',2,1,1},.type=2,.machine=62,.version=1,
        .entry=SYLE_KERNEL_BASE+1,.phoff=64,.ehsize=64,.phentsize=56,.phnum=3};
    struct ph p[] = {
        {1,5,0x1000,SYLE_KERNEL_BASE,SYLE_KERNEL_BASE,0x20,0x40,4096},
        {1,6,0x2000,SYLE_KERNEL_BASE+0x2000,SYLE_KERNEL_BASE+0x2000,0x30,0x1080,4096},
        {0x6474e551,6,0,0,0,0,0,16}
    };
    memcpy(image,&h,sizeof h); memcpy(image+64,p,sizeof p);
    memset(image+0x1000,0x55,0x20); memset(image+0x2000,0x66,0x30);
}
static void reject(void) {
    struct syle_kernel_plan p={1,2,3};
    memset(output,0xa5,sizeof output);
    assert(syle_kernel_elf_inspect(image,sizeof image,&p)==-1);
    assert(p.base==1 && p.size==2 && p.entry==3);
    assert(syle_kernel_elf_load(image,sizeof image,output,sizeof output,&p)==-1);
    assert(p.base==1 && p.size==2 && p.entry==3);
    for(size_t i=0;i<sizeof output;++i) assert(output[i]==0xa5);
}
#define BAD_H(member,value) do {valid();struct eh h;memcpy(&h,image,sizeof h);h.member=(value);memcpy(image,&h,sizeof h);reject();} while(0)
#define BAD_P(n,member,value) do {valid();struct ph p;memcpy(&p,image+64+(n)*56,sizeof p);p.member=(value);memcpy(image+64+(n)*56,&p,sizeof p);reject();} while(0)
static unsigned reservations, releases;
static int reserve_error, wrong_address, null_destination, overlap_destination;
static uint64_t reserved_address, released_address;
static size_t reserved_bytes, released_bytes;
static int reserve(void *ctx, uint64_t *address, size_t bytes, void **destination) {
    (void)ctx; assert(bytes<=sizeof output); ++reservations;
    if(reserve_error) return -1;
    if(wrong_address) *address+=4096;
    reserved_address=*address;reserved_bytes=bytes;
    *destination=null_destination ? NULL : overlap_destination ? image : output;
    return 0;
}
static void release(void *ctx,uint64_t address,size_t bytes) {
    (void)ctx; ++releases;
    released_address=address;released_bytes=bytes;
    assert(address==reserved_address && bytes==reserved_bytes);
}
int main(int argc,char **argv) {
    struct syle_kernel_plan p;
    valid();
    assert(!syle_kernel_elf_inspect(image,sizeof image,&p));
    assert(p.base==SYLE_KERNEL_BASE && p.size==0x4000 && p.entry==SYLE_KERNEL_BASE+1);
    memset(output,0xa5,sizeof output);
    assert(!syle_kernel_elf_load(image,sizeof image,output,sizeof output,&p));
    for(size_t i=0;i<0x4000;++i) {
        unsigned want = i<0x20 ? 0x55 : i>=0x2000 && i<0x2030 ? 0x66 : 0;
        assert(output[i]==want);
    }
    assert(output[0x4000]==0xa5);
    assert(syle_kernel_elf_load(image,sizeof image,output,0x3fff,&p)==-1);
    assert(syle_kernel_elf_load(image,sizeof image,image,0x4000,&p)==-1);
    assert(syle_kernel_elf_inspect(image,63,&p)==-1);
    assert(syle_kernel_elf_inspect(image,SYLE_KERNEL_MAX_FILE+1,&p)==-1);
    assert(syle_kernel_elf_inspect(NULL,sizeof image,&p)==-1);
    assert(syle_kernel_elf_inspect(image,sizeof image,NULL)==-1);
    p=(struct syle_kernel_plan){1,2,3};memset(output,0xa5,sizeof output);
    assert(syle_kernel_elf_load(image,sizeof image,NULL,sizeof output,&p)==-1);
    assert(p.base==1 && p.size==2 && p.entry==3);
    assert(syle_kernel_elf_load(image,sizeof image,output,sizeof output,NULL)==-1);
    assert(syle_kernel_elf_load(image,sizeof image,output,0x3fff,&p)==-1);
    assert(p.base==1 && p.size==2 && p.entry==3);
    for(size_t i=0;i<sizeof output;++i) assert(output[i]==0xa5);
    BAD_H(ident[0],0);BAD_H(ident[4],1);BAD_H(ident[5],2);BAD_H(type,3);BAD_H(machine,3);
    BAD_H(ident[1],0);BAD_H(ident[2],0);BAD_H(ident[3],0);BAD_H(ident[6],0);
    BAD_H(version,0);BAD_H(flags,1);BAD_H(ehsize,0);BAD_H(phentsize,55);
    BAD_H(phnum,0);BAD_H(phnum,17);BAD_H(phoff,UINT64_MAX);BAD_H(phoff,sizeof image-55);
    BAD_H(phoff,63);BAD_H(entry,SYLE_KERNEL_BASE-1);
    BAD_H(entry,SYLE_KERNEL_BASE+0x20); /* executable BSS is not valid entry */
    BAD_H(entry,SYLE_KERNEL_BASE+0x2000);
    BAD_P(0,type,2);BAD_P(0,type,3);BAD_P(0,type,6);
    BAD_P(0,flags,7);BAD_P(0,flags,0);BAD_P(0,memsz,0);BAD_P(0,filesz,0x41);
    BAD_P(0,offset,UINT64_MAX);BAD_P(0,offset,sizeof image-1);
    BAD_P(0,paddr,SYLE_KERNEL_BASE+4096);BAD_P(0,vaddr,UINT64_MAX);
    BAD_P(0,memsz,UINT64_MAX);BAD_P(0,memsz,SYLE_KERNEL_MAX_SPAN+1);
    BAD_P(0,align,3);BAD_P(0,align,8192);BAD_P(2,flags,7);
    /* Overlap at byte and page level must both fail. */
    valid();struct ph q;memcpy(&q,image+64+56,sizeof q);
    q.vaddr=q.paddr=SYLE_KERNEL_BASE;q.align=1;memcpy(image+64+56,&q,sizeof q);reject();
    valid();memcpy(&q,image+64+56,sizeof q);
    q.vaddr=q.paddr=SYLE_KERNEL_BASE+0x100;q.align=1;memcpy(image+64+56,&q,sizeof q);reject();
    /* Entire file must validate before any segment is copied. */
    BAD_P(1,flags,7);
    /* Null/optional headers accepted; no GNU_STACK requirement. */
    valid();memcpy(&q,image+64+112,sizeof q);q.type=0;memcpy(image+64+112,&q,sizeof q);
    assert(!syle_kernel_elf_inspect(image,sizeof image,&p));
    /* Header order is not address order; the same image must still load. */
    valid();struct ph first,second;
    memcpy(&first,image+64,sizeof first);memcpy(&second,image+120,sizeof second);
    memcpy(image+64,&second,sizeof second);memcpy(image+120,&first,sizeof first);
    memset(output,0xa5,sizeof output);
    assert(!syle_kernel_elf_load(image,sizeof image,output,sizeof output,&p));
    assert(p.base==SYLE_KERNEL_BASE && p.size==0x4000 && p.entry==SYLE_KERNEL_BASE+1);
    for(size_t i=0;i<sizeof output;++i) {
        unsigned want=i>=0x4000 ? 0xa5 : i<0x20 ? 0x55 : i>=0x2000 && i<0x2030 ? 0x66 : 0;
        assert(output[i]==want);
    }
    /* Duplicate stack, zero loads, and a missing base segment are rejected. */
    valid();memcpy(&q,image+176,sizeof q);memcpy(image+120,&q,sizeof q);reject();
    valid();struct eh no_load;memcpy(&no_load,image,sizeof no_load);no_load.phnum=1;
    memcpy(image,&no_load,sizeof no_load);memcpy(&q,image+64,sizeof q);q.type=0;
    memcpy(image+64,&q,sizeof q);reject();
    valid();memcpy(&q,image+64,sizeof q);q.type=0;memcpy(image+64,&q,sizeof q);reject();
    /* Change both address fields so these reach the actual range checks. */
    valid();memcpy(&q,image+64,sizeof q);q.vaddr=q.paddr=SYLE_KERNEL_BASE-1;
    memcpy(image+64,&q,sizeof q);reject();
    valid();memcpy(&q,image+64,sizeof q);q.vaddr=q.paddr=SYLE_KERNEL_BASE+SYLE_KERNEL_MAX_SPAN;
    memcpy(image+64,&q,sizeof q);reject();
    /* Ordinary allocated buffers cover both overlap directions without
     * fabricated pointers, address mappings, or executing any loaded code. */
    unsigned char arena[0xb000],snapshot[sizeof arena];
    for(unsigned direction=0;direction<2;++direction) {
        valid();memset(arena,0xa5,sizeof arena);memcpy(arena+0x4000,image,sizeof image);
        memcpy(snapshot,arena,sizeof arena);p=(struct syle_kernel_plan){1,2,3};
        unsigned char *destination=arena+(direction ? 0x5000 : 0x3000);
        assert(syle_kernel_elf_load(arena+0x4000,sizeof image,destination,0x4000,&p)==-1);
        assert(!memcmp(arena,snapshot,sizeof arena) && p.base==1 && p.size==2 && p.entry==3);
    }
    valid();memset(arena,0xa5,sizeof arena);memcpy(arena+0x4000,image,sizeof image);
    assert(!syle_kernel_elf_load(arena+0x4000,sizeof image,arena,0x4000,&p));
    assert(arena[0]==0x55 && arena[0x2000]==0x66 && arena[0x3fff]==0);
    assert(!memcmp(arena+0x4000,image,sizeof image));
    /* File and physical-span limits are independent and inclusive. */
    unsigned char *large=calloc(1,SYLE_KERNEL_MAX_FILE); assert(large);
    valid(); memcpy(large,image,sizeof image);
    assert(!syle_kernel_elf_inspect(large,SYLE_KERNEL_MAX_FILE,&p));
    assert(syle_kernel_elf_inspect(large,SYLE_KERNEL_MAX_FILE+1,&p)==-1);
    free(large);
    valid();memcpy(&q,image+64+56,sizeof q);
    q.memsz=SYLE_KERNEL_MAX_SPAN-0x2000;memcpy(image+64+56,&q,sizeof q);
    assert(!syle_kernel_elf_inspect(image,sizeof image,&p));
    assert(p.size==SYLE_KERNEL_MAX_SPAN);
    unsigned char *span=malloc(p.size+1);assert(span);memset(span,0xa5,p.size+1);
    assert(!syle_kernel_elf_load(image,sizeof image,span,p.size,&p));
    assert(span[p.size-1]==0 && span[p.size]==0xa5);free(span);
    q.memsz++;memcpy(image+64+56,&q,sizeof q);reject();
    /* A symbol table does not request dynamic linking. */
    valid();struct eh symbols;memcpy(&symbols,image,sizeof symbols);
    symbols.shoff=0x2800;symbols.shentsize=64;symbols.shnum=2;
    memcpy(image,&symbols,sizeof symbols);
    assert(!syle_kernel_elf_inspect(image,sizeof image,&p));
    const struct syle_kernel_allocator allocator={NULL,reserve,release};
    valid(); memset(output,0xa5,sizeof output); p=(struct syle_kernel_plan){1,2,3};
    reserve_error=1;
    assert(syle_kernel_prepare(image,sizeof image,&allocator,&p)==-2);
    assert(reservations==1 && releases==0 && p.base==1);
    for(size_t i=0;i<sizeof output;++i) assert(output[i]==0xa5);
    reserve_error=0;wrong_address=1;
    assert(syle_kernel_prepare(image,sizeof image,&allocator,&p)==-3);
    assert(reservations==2 && releases==1 && p.base==1);
    assert(p.size==2 && p.entry==3 && released_address==SYLE_KERNEL_BASE+4096 && released_bytes==0x4000);
    for(size_t i=0;i<sizeof output;++i) assert(output[i]==0xa5);
    wrong_address=0;
    assert(!syle_kernel_prepare(image,sizeof image,&allocator,&p));
    assert(reservations==3 && releases==1 && output[0]==0x55);
    BAD_P(1,flags,7);
    assert(syle_kernel_prepare(image,sizeof image,&allocator,&p)==-1);
    assert(reservations==3); /* Later bad segment causes no allocation. */
    valid();p=(struct syle_kernel_plan){1,2,3};memset(output,0xa5,sizeof output);
    null_destination=1;
    assert(syle_kernel_prepare(image,sizeof image,&allocator,&p)==-3);
    assert(reservations==4 && releases==2 && p.base==1 && p.size==2 && p.entry==3);
    assert(released_address==SYLE_KERNEL_BASE && released_bytes==0x4000);
    for(size_t i=0;i<sizeof output;++i) assert(output[i]==0xa5);
    null_destination=0;overlap_destination=1;
    unsigned char saved_image[sizeof image];memcpy(saved_image,image,sizeof image);
    assert(syle_kernel_prepare(image,sizeof image,&allocator,&p)==-3);
    assert(reservations==5 && releases==3 && p.base==1 && p.size==2 && p.entry==3);
    assert(!memcmp(image,saved_image,sizeof image));
    for(size_t i=0;i<sizeof output;++i) assert(output[i]==0xa5);
    overlap_destination=0;
    if(argc==2) {
        FILE *f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0);
        assert(!fseek(f,0,SEEK_SET));unsigned char *file=malloc((size_t)n);assert(file);
        assert(fread(file,1,(size_t)n,f)==(size_t)n);fclose(f);
        assert(!syle_kernel_elf_inspect(file,(size_t)n,&p));
        unsigned char *dest=malloc((size_t)p.size);assert(dest);
        assert(!syle_kernel_elf_load(file,(size_t)n,dest,(size_t)p.size,&p));
        printf("kernel ELF artifact: base=%llx span=%llu entry=%llx PASS\n",
            (unsigned long long)p.base,(unsigned long long)p.size,(unsigned long long)p.entry);
        free(dest);free(file);
    }
    puts("kernel ELF parser/load bounds, permissions, overlaps, zero-fill PASS");
}
