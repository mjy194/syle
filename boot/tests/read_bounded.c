#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../read_bounded.h"
struct fake { size_t bytes, continuation; unsigned reads, fail_at, overreport; };
static int read(void *ctx, void *buffer, size_t *bytes) {
    struct fake *f=ctx;
    if (++f->reads==f->fail_at) return -1;
    if(f->reads==1 && f->overreport) { ++*bytes; return 0; }
    size_t n=f->reads==1 ? f->bytes : f->continuation;
    assert(n<=*bytes);memset(buffer,'K',n);*bytes=n;return 0;
}
int main(void) {
    unsigned char buffer[17]; size_t used=99;
    struct fake f={.bytes=16};memset(buffer,0xa5,sizeof buffer);
    assert(!syle_read_bounded(&f,read,buffer,16,&used) && used==16 && buffer[16]==0xa5);
    assert(f.reads==2);
    f=(struct fake){.bytes=8,.continuation=1};used=99;
    assert(syle_read_bounded(&f,read,buffer,16,&used)==-1 && used==99);
    assert(f.reads==2 && buffer[16]==0xa5);
    f=(struct fake){.bytes=16,.continuation=1};
    assert(syle_read_bounded(&f,read,buffer,16,&used)==-1 && used==99);
    for(unsigned i=1;i<=2;++i) {
        f=(struct fake){.bytes=8,.fail_at=i};
        assert(syle_read_bounded(&f,read,buffer,16,&used)==-1 && used==99);
        assert(f.reads==i && buffer[16]==0xa5);
    }
    /* A lying callback must not publish its size or trigger a second read. */
    f=(struct fake){.overreport=1};used=99;
    memset(buffer,0xa5,sizeof buffer);
    assert(syle_read_bounded(&f,read,buffer,16,&used)==-1 && used==99 && f.reads==1);
    for(size_t i=0;i<sizeof buffer;++i) assert(buffer[i]==0xa5);
    /* EOF after a short first read is valid, including the empty stream. */
    for(size_t n=0;n<=8;n+=8) {
        f=(struct fake){.bytes=n};used=99;memset(buffer,0xa5,sizeof buffer);
        assert(!syle_read_bounded(&f,read,buffer,16,&used) && used==n && f.reads==2);
        for(size_t i=0;i<n;++i) assert(buffer[i]=='K');
        for(size_t i=n;i<sizeof buffer;++i) assert(buffer[i]==0xa5);
    }
    f=(struct fake){0};used=99;
    assert(!syle_read_bounded(&f,read,buffer,0,&used) && !used && f.reads==2);
    puts("DRAFT-MS1-011/014: bounded complete read, partial continuation, I/O failures PASS");
}
