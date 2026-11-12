#include "../smp_acpi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define R 0x100
#define X 0x1000
#define M 0x2000
static uint8_t mem[0x20000];
static size_t available;
static void w32(unsigned p,uint32_t v) { for(unsigned i=0;i<4;i++)mem[p+i]=(uint8_t)(v>>(8*i)); }
static void w64(unsigned p,uint64_t v) { w32(p,(uint32_t)v);w32(p+4,(uint32_t)(v>>32)); }
static bool readmem(void *ctx,uint64_t p,void *d,size_t n)
{
    (void)ctx;
    if(p>available || n>available-p)return false;
    memcpy(d,mem+(size_t)p,n);return true;
}
static void fix(unsigned p,unsigned n,unsigned c)
{
    mem[p+c]=0;unsigned sum=0;
    for(unsigned i=0;i<n;i++)sum+=mem[p+i];
    mem[p+c]=(uint8_t)(-sum);
}
static void table(unsigned p,const char *sig,unsigned n)
{ memcpy(mem+p,sig,4);w32(p+4,n);mem[p+8]=1;fix(p,n,9); }
static void base(bool extended)
{
    memset(mem,0,sizeof(mem));available=sizeof(mem);
    memcpy(mem+R,"RSD PTR ",8);w32(R+16,X);
    if(extended){mem[R+15]=2;w32(R+20,36);w64(R+24,X);}
    table(X,extended?"XSDT":"RSDT",extended?44:40);
    if(extended)w64(X+36,M);else w32(X+36,M);
    fix(X,extended?44:40,9);
    w32(M+36,0xfee00000);
    mem[M+44]=0;mem[M+45]=8;mem[M+47]=2;w32(M+48,1);
    mem[M+52]=0;mem[M+53]=8;mem[M+55]=7;w32(M+56,1);
    table(M,"APIC",60);fix(R,20,8);if(extended)fix(R,36,32);
}
static bool run(struct smp_acpi_topology *t)
{ return smp_acpi_discover(R,2,readmem,0,t); }
static void bad(void)
{
    struct smp_acpi_topology t,old;
    memset(&t,0xa5,sizeof(t));memcpy(&old,&t,sizeof(t));
    assert(!run(&t));assert(!memcmp(&old,&t,sizeof(t)));
}
int main(void)
{
    struct smp_acpi_topology t;
    base(false);assert(run(&t));assert(!t.used_xsdt&&t.ap_id==7&&t.bsp_id==2&&t.enabled_count==2&&t.lapic_base==0xfee00000);
    base(true);assert(run(&t)&&t.used_xsdt);
    /* v2 without XSDT falls back to valid RSDT. */
    base(false);mem[R+15]=2;w32(R+20,36);fix(R,20,8);fix(R,36,32);assert(run(&t)&&!t.used_xsdt);
    base(true);mem[R+8]++;bad();
    base(true);mem[R+32]++;bad();
    base(true);w32(R+20,4097);fix(R,36,32);bad();
    base(true);w32(R+20,35);fix(R,36,32);bad();
    base(true);mem[R+15]=1;fix(R,20,8);bad();
    base(true);mem[X+9]++;bad();
    base(true);mem[M+9]++;bad();
    base(true);available=M+59;bad();
    base(true);w64(R+24,UINT64_MAX-10);fix(R,36,32);bad();
    base(true);w64(X+36,UINT64_MAX-35);fix(X,44,9);bad();
    base(true);w32(X+4,65537);bad();
    base(true);table(X,"XSDT",45);bad();
    base(true);table(X,"XSDT",36+257*8);bad();
    base(true);w64(X+44,M);table(X,"XSDT",52);bad(); /* duplicate MADT */
    base(true);mem[M+55]=2;fix(M,60,9);bad();
    base(true);mem[M+47]=3;fix(M,60,9);bad(); /* BSP absent */
    base(true);w32(M+56,0);fix(M,60,9);bad();
    base(true);mem[M+53]=0;fix(M,60,9);bad();
    base(true);mem[M+53]=7;fix(M,60,9);bad();
    base(true);mem[M+53]=9;fix(M,60,9);bad();
    base(true);mem[M+60]=42;mem[M+61]=2;table(M,"APIC",62);assert(run(&t));
    base(true);table(M,"APIC",61);bad();
    base(true);mem[M+60]=5;mem[M+61]=12;w64(M+64,0xfee01000);table(M,"APIC",72);assert(run(&t)&&t.lapic_base==0xfee01000);
    w64(M+64,0xfee01001);fix(M,72,9);bad();
    w64(M+64,1ull<<52);fix(M,72,9);bad();
    w64(M+64,0xfee01000);mem[M+62]=1;fix(M,72,9);bad();
    base(true);mem[M+60]=5;mem[M+61]=12;w64(M+64,0xfee01000);memcpy(mem+M+72,mem+M+60,12);table(M,"APIC",84);bad();
    /* x2APIC records are usable only with xAPIC-compatible IDs. */
    base(true);mem[M+52]=9;mem[M+53]=16;w32(M+56,7);w32(M+60,1);table(M,"APIC",68);assert(run(&t));
    w32(M+56,256);fix(M,68,9);bad();
    w32(M+56,2);fix(M,68,9);bad();
    base(true);mem[M+60]=9;mem[M+61]=16;w32(M+64,999);table(M,"APIC",76);assert(run(&t)); /* disabled ignored */
    base(true);for(unsigned i=0;i<255;i++){mem[M+60+2*i]=42;mem[M+61+2*i]=2;}table(M,"APIC",570);bad();
    base(true);assert(!smp_acpi_discover(UINT64_MAX,2,readmem,0,&t));
    assert(!smp_acpi_discover(R,256,readmem,0,&t));
    assert(!smp_acpi_discover(R,2,0,0,&t));
    assert(!smp_acpi_discover(R,2,readmem,0,0));
    /* Single-CPU API never weakens the old exact-two contract. */
    base(true);table(M,"APIC",52);
    assert(smp_acpi_discover_single(R,2,readmem,0,&t));
    assert(t.enabled_count==1 && t.bsp_id==2 && t.ap_id==0 && t.used_xsdt);
    bad(); /* existing discover still requires two */
    struct smp_acpi_topology saved=t;
    assert(!smp_acpi_discover_single(R,7,readmem,0,&t));
    assert(!memcmp(&t,&saved,sizeof t));
    base(false);table(M,"APIC",52);
    assert(smp_acpi_discover_single(R,2,readmem,0,&t));
    assert(!t.used_xsdt);
    base(true);saved=t;
    assert(!smp_acpi_discover_single(R,2,readmem,0,&t)); /* two enabled records */
    assert(!memcmp(&t,&saved,sizeof t));
    mem[M+55]=2;fix(M,60,9);
    assert(!smp_acpi_discover_single(R,2,readmem,0,&t)); /* duplicate enabled BSP */
    base(true);w32(M+56,0);fix(M,60,9);
    assert(smp_acpi_discover_single(R,2,readmem,0,&t)); /* disabled AP not counted */
    w32(M+56,2);fix(M,60,9);
    assert(!smp_acpi_discover_single(R,2,readmem,0,&t)); /* disabled but online-capable */
    w32(M+56,0);w32(M+48,0);fix(M,60,9);
    assert(!smp_acpi_discover_single(R,2,readmem,0,&t)); /* none enabled */
    puts("smp_acpi: PASS");
}
