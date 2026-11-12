#include "smp_acpi.h"

#define TABLE_LIMIT 65536u
#define ROOT_LIMIT 256u
#define RECORD_LIMIT 256u
struct reader { smp_acpi_read_fn read; void *ctx; };
static uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 |
           (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static uint64_t u64(const uint8_t *p)
{
    return u32(p) | (uint64_t)u32(p+4)<<32;
}
static bool bytes(struct reader *r, uint64_t pa, void *dst, size_t n)
{
    return n && pa <= UINT64_MAX-(n-1) && r->read(r->ctx,pa,dst,n);
}
static bool equal(const uint8_t *p, const char *s, unsigned n)
{
    for (unsigned i=0;i<n;i++) if (p[i]!=(uint8_t)s[i]) return false;
    return true;
}
static bool checksum(struct reader *r, uint64_t pa, uint32_t n)
{
    uint8_t buf[64], sum=0;
    if (!n || pa>UINT64_MAX-(n-1)) return false;
    for (uint32_t off=0;off<n;) {
        unsigned len=n-off>sizeof(buf)?sizeof(buf):n-off;
        if (!bytes(r,pa+off,buf,len)) return false;
        for (unsigned i=0;i<len;i++) sum=(uint8_t)(sum+buf[i]);
        off+=len;
    }
    return sum==0;
}
static bool header(struct reader *r, uint64_t pa, uint8_t h[36], uint32_t *len)
{
    if (!pa || !bytes(r,pa,h,36)) return false;
    *len=u32(h+4);
    return *len>=36 && *len<=TABLE_LIMIT && checksum(r,pa,*len);
}
static bool lapic_valid(uint64_t pa)
{
    /* xAPIC MMIO is one page; reject addresses beyond architectural 52 bits. */
    return pa && !(pa&4095) && pa<((uint64_t)1<<52);
}
static bool madt(struct reader *r, uint64_t pa, uint32_t len,
                 uint32_t bsp, unsigned expected, struct smp_acpi_topology *top)
{
    uint8_t b[16];
    uint32_t ids[2]={0,0};
    unsigned count=0, records=0;
    bool override=false;
    if (len<44 || !bytes(r,pa+36,b,8)) return false;
    uint64_t lapic=u32(b);
    for (uint32_t off=44;off<len;) {
        if (++records>RECORD_LIMIT || len-off<2 || !bytes(r,pa+off,b,2)) return false;
        unsigned type=b[0], size=b[1];
        if (size<2 || size>len-off) return false;
        unsigned needed=type==0?8:type==5?12:type==9?16:2;
        if (size<needed || !bytes(r,pa+off,b,needed)) return false;
        if (type==0 || type==9) {
            uint32_t id=type==0?b[3]:u32(b+4);
            uint32_t flags=type==0?u32(b+4):u32(b+8);
            /* Single-CPU cache-transition profile excludes potential hotplug
             * CPUs too. Do not alter the old two-CPU discovery contract. */
            if(expected==1 && !(flags&1) && (flags&2))return false;
            if (flags&1) {
                if (id>255 || count==expected) return false;
                for (unsigned i=0;i<count;i++) if (ids[i]==id) return false;
                ids[count++]=id;
            }
        } else if (type==5) {
            if (override || b[2] || b[3]) return false;
            override=true;
            lapic=u64(b+4);
        }
        off+=size;
    }
    if (!lapic_valid(lapic) || count!=expected ||
        (ids[0]!=bsp && (count<2 || ids[1]!=bsp))) return false;
    top->lapic_base=lapic;
    top->bsp_id=bsp;
    top->ap_id=count==1?0:(ids[0]==bsp?ids[1]:ids[0]);
    top->enabled_count=count;
    return true;
}
static bool discover(uint64_t pa, uint32_t bsp, smp_acpi_read_fn read,
                       void *ctx, struct smp_acpi_topology *out,unsigned expected)
{
    struct reader r={read,ctx};
    struct smp_acpi_topology top={0};
    uint8_t b[36], h[36];
    if (!read || !out || bsp>255 || !bytes(&r,pa,b,20) ||
        !equal(b,"RSD PTR ",8) || !checksum(&r,pa,20)) return false;
    uint64_t root=u32(b+16);
    if (b[15]) {
        if (b[15]<2 || !bytes(&r,pa,b,36)) return false;
        uint32_t n=u32(b+20);
        if (n<36 || n>4096 || !checksum(&r,pa,n)) return false;
        uint64_t xsdt=u64(b+24);
        if (xsdt) { root=xsdt; top.used_xsdt=true; }
    }
    uint32_t length;
    if (!header(&r,root,h,&length) ||
        !equal(h,top.used_xsdt?"XSDT":"RSDT",4)) return false;
    unsigned width=top.used_xsdt?8:4;
    if ((length-36)%width || (length-36)/width>ROOT_LIMIT) return false;
    uint64_t found=0;
    uint32_t foundlen=0;
    for (unsigned off=36;off<length;off+=width) {
        if (!bytes(&r,root+off,b,width)) return false;
        uint64_t entry=width==8?u64(b):u32(b);
        uint32_t n;
        if (!header(&r,entry,h,&n)) return false;
        if (equal(h,"APIC",4)) {
            if (found) return false;
            found=entry; foundlen=n;
        }
    }
    if (!found || !madt(&r,found,foundlen,bsp,expected,&top)) return false;
    *out=top;
    return true;
}

bool smp_acpi_discover(uint64_t pa,uint32_t bsp,smp_acpi_read_fn read,
                       void *ctx,struct smp_acpi_topology *out)
{ return discover(pa,bsp,read,ctx,out,2); }
bool smp_acpi_discover_single(uint64_t pa,uint32_t bsp,smp_acpi_read_fn read,
                              void *ctx,struct smp_acpi_topology *out)
{ return discover(pa,bsp,read,ctx,out,1); }
