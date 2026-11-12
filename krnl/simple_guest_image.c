#include "simple_guest.h"

/* Read fields explicitly: ELF input may be unaligned and is untrusted. */
static uint64_t field(const unsigned char *p, unsigned n) {
    uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= (uint64_t)p[i] << (8*i);
    return v;
}
bool sg_image_executable(const struct sg_image *im, uint64_t rip, uint64_t length) {
    if (!im || !length || im->count > SG_MAX_SEGMENTS) return false;
    for (unsigned i = 0; i < im->count; ++i) {
        const struct sg_segment *s = &im->segments[i];
        if (s->flags == 5 && rip >= s->address &&
            s->filesz <= UINT64_MAX - s->address &&
            rip - s->address < s->filesz &&
            length <= s->filesz - (rip - s->address)) return true;
    }
    return false;
}
bool sg_image_inspect(const void *input, size_t size, struct sg_image *out) {
    const unsigned char *b = input;
    struct sg_image im = {0};
    if (!b || !out || size < 64 || size > SG_MAX_IMAGE ||
        (uintptr_t)b > UINTPTR_MAX - size) return false;
    if (b[0]!=0x7f || b[1]!='E' || b[2]!='L' || b[3]!='F' ||
        b[4]!=2 || b[5]!=1 || b[6]!=1 || b[7]!=0 || b[8]!=0) return false;
    for (unsigned i=9;i<16;++i) if (b[i]) return false;
    if (field(b+16,2)!=2 || field(b+18,2)!=62 || field(b+20,4)!=1 ||
        field(b+48,4)!=0 || field(b+52,2)!=64 || field(b+54,2)!=56) return false;
    uint64_t ph = field(b+32,8), count = field(b+56,2);
    if (!count || count > SG_MAX_SEGMENTS+1 || ph < 64 || ph > size || count > (size-ph)/56) return false;
    /* Section metadata is not interpreted, but any declared table must fit. */
    uint64_t sh = field(b+40,8), sn = field(b+60,2), si = field(b+62,2);
    if (sn) {
        if (field(b+58,2)!=64 || sh < 64 || sh > size || sn > (size-sh)/64 || si >= sn) return false;
    } else if (sh || si) return false;
    im.entry = field(b+24,8);
    bool stack = false;
    for (uint64_t i=0;i<count;++i) {
        const unsigned char *p = b+ph+i*56;
        uint64_t type=field(p,4), flags=field(p+4,4), align=field(p+48,8);
        if (type==UINT64_C(0x6474e551)) {
            if (stack || flags!=6 || field(p+8,8) || field(p+16,8) || field(p+24,8) || field(p+32,8) || field(p+40,8) ||
                (align && (align & (align-1)))) return false;
            stack=true; continue;
        }
        if (type!=1 || im.count==SG_MAX_SEGMENTS || (flags!=4 && flags!=5 && flags!=6)) return false;
        struct sg_segment s = {field(p+8,8),field(p+16,8),field(p+32,8),field(p+40,8),(uint32_t)flags};
        if (s.address!=field(p+24,8) || !s.memsz || s.filesz>s.memsz ||
            s.address<SG_IMAGE_MIN || s.address>=SG_RAM_SIZE || s.memsz>SG_RAM_SIZE-s.address ||
            s.offset>size || s.filesz>size-s.offset ||
            (align>1 && ((align & (align-1)) || (s.address % align)!=(s.offset % align))) ||
            (s.address % SG_PAGE_SIZE)!=(s.offset % SG_PAGE_SIZE)) return false;
        uint64_t first=s.address/SG_PAGE_SIZE, last=(s.address+s.memsz-1)/SG_PAGE_SIZE;
        for (unsigned j=0;j<im.count;++j) {
            const struct sg_segment *a=&im.segments[j];
            if (first <= (a->address+a->memsz-1)/SG_PAGE_SIZE && last >= a->address/SG_PAGE_SIZE) return false;
        }
        im.segments[im.count++]=s;
    }
    if (!im.count || !sg_image_executable(&im,im.entry,1)) return false;
    *out=im;
    return true;
}
