#include "../nested_ept_map.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t tables[4][512] __attribute__((aligned(4096)));
struct source { uint8_t leaf_perm; bool missing,high_hpa; unsigned calls; };
static bool resolve(void *context,uint64_t page,uint8_t need,uint64_t *hpa,
                    const uint64_t **pointer,uint8_t *perm) {
    struct source *s=context;s->calls++;
    assert(need==1); /* complete plans always read-walk */
    if(page>=0x1000 && page<=0x4000 && !(page&0xfff)) {
        unsigned i=(unsigned)(page/4096)-1;
        *hpa=0x100000+page;*pointer=tables[i];*perm=7;return true;
    }
    if(page!=0x8000 || s->missing) return false;
    *hpa=s->high_hpa?(UINT64_C(1)<<48):0x800000;
    *pointer=NULL;*perm=s->leaf_perm;return true;
}
static struct source init(void) {
    memset(tables,0,sizeof tables);
    tables[0][0]=0x2007;tables[1][0]=0x3007;tables[2][0]=0x4007;
    tables[3][0]=0x8037;
    tables[3][15]=0x8037;
    tables[3][63]=0x8037;
    return (struct source){.leaf_perm=7};
}
static void unchanged_failure(struct nested_ept_map *map,struct source *s) {
    struct nested_ept_map old=*map;
    assert(!nested_ept_map_build(map,0x101e,resolve,s));
    assert(memcmp(map,&old,sizeof old)==0);
}
int main(void) {
    struct source s=init();
    struct nested_ept_map map;
    memset(&map,0xa5,sizeof map);
    assert(nested_ept_map_build(&map,0x101e,resolve,&s));
    assert(map.leaves[0]==0x800037 && map.leaves[15]==map.leaves[0]);
    assert(map.leaves[63]==map.leaves[0] && map.leaves[62]==0);
    struct nested_ept_result r=nested_ept_map_classify(&map,0xf123,1);
    assert(r.status==NESTED_EPT_OK && r.l1_gpa==0x8123 && r.hpa==0x800123);
    r=nested_ept_map_classify(&map,0x3ffff,4);
    assert(r.status==NESTED_EPT_OK && r.hpa==0x800fff);
    assert(nested_ept_map_classify(&map,0x40000,1).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    assert(nested_ept_map_classify(&map,UINT64_C(0x100000000000f000),1).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    assert(nested_ept_map_classify(&map,UINT64_MAX,1).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    assert(nested_ept_map_classify(&map,0,3).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    assert(nested_ept_map_classify(NULL,0,1).status==NESTED_EPT_MALFORMED_UNSUPPORTED);
    assert(nested_ept_map_classify(&map,0x1000,1).status==NESTED_EPT_L1_DENIED);

    /* A rebuild clears stale EPT02 leaves, including zero-permission junk. */
    tables[3][15]=UINT64_C(0xfffffffffffffff8);
    assert(nested_ept_map_build(&map,0x101e,resolve,&s));
    assert(map.leaves[15]==0 && !map.pages[15].leaf_resolved);
    assert(nested_ept_map_classify(&map,0xf123,2).status==NESTED_EPT_L1_DENIED);
    tables[0][0]=0;
    assert(nested_ept_map_build(&map,0x101e,resolve,&s));
    for(unsigned i=0;i<64;i++) assert(map.leaves[i]==0);

    /* Upper EPT12 permissions are intersected into every alias. */
    s=init();tables[1][0]&=~UINT64_C(2);
    assert(nested_ept_map_build(&map,0x101e,resolve,&s));
    assert(map.leaves[0]==0x800035 && map.pages[0].effective12==5);
    r=nested_ept_map_classify(&map,0xf123,2);
    assert(r.status==NESTED_EPT_L1_DENIED && r.effective02==5);
    assert(nested_ept_map_classify(&map,0xf123,4).status==NESTED_EPT_OK);

    /* Same EPT02 RO result, but L0 denial must not be reflected as L1 denial. */
    s=init();s.leaf_perm=1;
    assert(nested_ept_map_build(&map,0x101e,resolve,&s));
    assert(map.leaves[15]==0x800031);
    r=nested_ept_map_classify(&map,0xf123,2);
    assert(r.status==NESTED_EPT_L0_BACKING_DENIED && r.effective12==7 && r.effective02==1);
    assert(nested_ept_map_classify(&map,0xf123,1).status==NESTED_EPT_OK);

    /* Failure at the final page cannot publish earlier valid pages. */
    s=init();tables[3][63]|=1u<<7;unchanged_failure(&map,&s);
    s=init();tables[3][63]=(UINT64_C(1)<<48)|0x8037;unchanged_failure(&map,&s);
    s=init();tables[3][63]=0x8032;unchanged_failure(&map,&s); /* W-only */
    s=init();tables[3][63]=0x8034;unchanged_failure(&map,&s); /* X-only */
    s=init();s.high_hpa=true;unchanged_failure(&map,&s);
    s=init();s.missing=true;unchanged_failure(&map,&s);
    s=init();s.leaf_perm=0;unchanged_failure(&map,&s);
    struct nested_ept_map old=map;
    assert(!nested_ept_map_build(&map,0x105e,resolve,&s)); /* A/D unsupported */
    assert(memcmp(&old,&map,sizeof map)==0);
    assert(!nested_ept_map_build(NULL,0x101e,resolve,&s));
    assert(!nested_ept_map_build(&map,0x101e,NULL,&s));
    assert(memcmp(&old,&map,sizeof map)==0);
    puts("PASS bounded EPT02 plan rebuild, classification and failure atomicity");
}
