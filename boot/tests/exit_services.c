#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../exit_services.h"
struct fake {
    unsigned maps, allocations, frees, exits, grow, stale;
    int map_error, exit_error, alloc_error, malformed, grow_after, bad_size;
    unsigned null_alloc_at, fail_alloc_at;
    int initial_map_error, map_error_after, ready_to_exit;
    size_t key;
    void *live;
};
static int map(void *context, void *buffer, size_t *bytes, size_t *key,
               size_t *stride, uint32_t *version) {
    struct fake *f = context;
    f->ready_to_exit=0;
    ++f->maps; *stride = 48; *version = 1;
    if(f->initial_map_error || (f->map_error_after && f->exits)) return SYLE_FW_ERROR;
    /* A success status is not sufficient without an allocated complete map. */
    if(f->malformed==3 && !buffer) { *bytes=48; return SYLE_FW_OK; }
    if(f->bad_size && !(f->bad_size==4 && buffer)) {
        *bytes = f->bad_size==1 ? SIZE_MAX :
                 f->bad_size==4 ? SYLE_MAP_MAX - 32 * 48 : SYLE_MAP_MAX;
        if(f->bad_size==3) *stride=SIZE_MAX;
        if(f->bad_size==5) *stride=32;
        if(f->bad_size==6) *bytes=0;
        return SYLE_FW_SMALL;
    }
    if (f->map_error && buffer) return SYLE_FW_ERROR;
    if (!buffer || (f->grow && f->allocations <= f->grow) ||
        (f->grow_after && f->exits)) {
        *bytes += 48;
        return SYLE_FW_SMALL;
    }
    *bytes = f->malformed ? 47 : 48;
    if (f->malformed == 2) { *stride = 41; *bytes = 82; }
    if (f->malformed == 4) *bytes=0;
    if (f->malformed == 5) *bytes=48+32*48+1;
    if (f->malformed == 6) { *stride=32; *bytes=48; }
    if (f->malformed == 7) *bytes=49;
    if (f->malformed == 8) { *bytes=48; *version=2; }
    *key = f->key = 100 + f->maps;
    f->ready_to_exit=!f->malformed;
    return SYLE_FW_OK;
}
static int allocate(void *context, size_t bytes, void **buffer) {
    struct fake *f = context;
    assert(!f->exits); ++f->allocations;
    if (f->alloc_error || f->allocations==f->fail_alloc_at) return -1;
    if (f->allocations==f->null_alloc_at) { *buffer=NULL; return 0; }
    *buffer = malloc(bytes); assert(*buffer); f->live = *buffer;
    return 0;
}
static void release(void *context, void *buffer) {
    struct fake *f = context;
    assert(!f->exits); ++f->frees; free(buffer);
    if (buffer == f->live) f->live = NULL;
}
static int leave(void *context, size_t key) {
    struct fake *f = context;
    assert(f->ready_to_exit && key == f->key); ++f->exits;
    f->ready_to_exit=0;
    if (f->exit_error) return SYLE_FW_ERROR;
    return f->exits <= f->stale ? SYLE_FW_STALE : SYLE_FW_OK;
}
static struct syle_exit_result run(struct fake *f, int expected) {
    struct syle_exit_ops ops = {f, map, allocate, release, leave};
    struct syle_exit_result result;
    assert(syle_exit_services(&ops, &result) == expected);
    assert(result.exited == (expected == 0));
    assert(result.attempted == f->exits);
    if (expected == 0) assert(result.key == f->key && result.bytes == 48);
    if(expected==-1 && !result.attempted) {
        assert(!result.map && !result.capacity && !f->live);
    } else {
        assert(result.map==f->live && result.map && result.capacity);
    }
    if (f->live) { free(f->live); f->live = NULL; }
    return result;
}
int main(void) {
    struct fake f = {0}; run(&f,0); assert(f.exits==1 && f.allocations==1);
    f=(struct fake){.grow=2};run(&f,0);assert(f.allocations==3 && f.frees==2);
    f=(struct fake){.grow=9};run(&f,-1);assert(f.allocations==3 && f.frees==3 && !f.exits);
    f=(struct fake){.stale=2};run(&f,0);assert(f.exits==3 && f.allocations==1);
    f=(struct fake){.stale=9};run(&f,-1);assert(f.exits==3 && !f.frees);
    f=(struct fake){.stale=1,.grow_after=1};run(&f,-1);assert(f.exits==1 && !f.frees && f.allocations==1);
    f=(struct fake){.map_error=1};run(&f,-1);assert(!f.exits && f.frees==1);
    f=(struct fake){.exit_error=1};run(&f,-1);assert(f.exits==1 && !f.frees);
    f=(struct fake){.alloc_error=1};run(&f,-1);assert(!f.exits && !f.frees);
    f=(struct fake){.malformed=1};run(&f,-1);assert(!f.exits && f.frees==1);
    f=(struct fake){.malformed=2};run(&f,-1);
    assert(!f.exits && f.frees==1); /* Complete but ABI-misaligned descriptors. */
    for(int i=1;i<=3;++i) {
        f=(struct fake){.bad_size=i};run(&f,-1);
        assert(!f.allocations && !f.exits);
    }
    f=(struct fake){.bad_size=4};
    struct syle_exit_result exact=run(&f,0);
    assert(exact.capacity==SYLE_MAP_MAX && f.allocations==1);
    /* Short stride and a non-growing size fail before any allocation. */
    for(int i=5;i<=6;++i) {
        f=(struct fake){.bad_size=i};run(&f,-1);
        assert(f.maps==1 && !f.allocations && !f.exits && !f.frees);
    }
    for(int i=3;i<=8;++i) {
        f=(struct fake){.malformed=i};
        struct syle_exit_result bad=run(&f,-1);
        assert(bad.status==SYLE_FW_ERROR && !f.exits);
        assert(f.allocations==(i==3 ? 0u : 1u) && f.frees==f.allocations);
    }
    f=(struct fake){.null_alloc_at=1};
    struct syle_exit_result null_allocation=run(&f,-1);
    assert(null_allocation.status==SYLE_FW_ERROR && f.maps==1 && f.allocations==1 && !f.frees);
    /* Failed replacement allocation must release only the old allocation. */
    f=(struct fake){.grow=1,.null_alloc_at=2};run(&f,-1);
    assert(f.maps==2 && f.allocations==2 && f.frees==1 && !f.exits);
    f=(struct fake){.grow=1,.fail_alloc_at=2};run(&f,-1);
    assert(f.maps==2 && f.allocations==2 && f.frees==1 && !f.exits);
    f=(struct fake){.initial_map_error=1};run(&f,-1);
    assert(f.maps==1 && !f.allocations && !f.frees && !f.exits);
    f=(struct fake){.stale=1,.map_error_after=1};
    struct syle_exit_result after=run(&f,-1);
    assert(after.status==SYLE_FW_ERROR && f.maps==3 && f.allocations==1 && f.exits==1 && !f.frees);
    puts("DRAFT-MS1-003/004/005: final map growth, stale key, bounded failure, post-attempt call safety PASS");
}
