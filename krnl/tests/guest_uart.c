#include "../guest_uart.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void rejected(struct guest_uart *u, unsigned offset, unsigned width,
                     bool write, uint32_t data) {
    struct guest_uart saved = *u;
    uint32_t value = data;
    bool emit = true;
    uint8_t byte = 0xa5;
    assert(!guest_uart_access(u, offset, width, write, &value, &emit, &byte));
    assert(!memcmp(u, &saved, sizeof saved));
    assert(value == data && emit && byte == 0xa5);
}
static void put(struct guest_uart *u, unsigned offset, uint8_t data) {
    uint32_t value = data;
    bool emit = true;
    uint8_t byte = 0xa5;
    assert(guest_uart_access(u, offset, 1, true, &value, &emit, &byte));
    assert(value == data && !emit && byte == 0);
}
static uint32_t get(struct guest_uart *u, unsigned offset) {
    struct guest_uart saved = *u;
    uint32_t value = UINT32_MAX;
    bool emit = true;
    uint8_t byte = 0xa5;
    assert(guest_uart_access(u, offset, 1, false, &value, &emit, &byte));
    assert(!emit && !byte && !memcmp(u, &saved, sizeof saved));
    return value;
}
int main(void) {
    struct guest_uart u;
    memset(&u, 0xa5, sizeof u);
    guest_uart_init(&u);
    guest_uart_init(0);
    const unsigned reset[8] = {0, 0, 1, 0, 0, 0x60, 0, 0};
    for (unsigned i=0; i<8; ++i) assert(get(&u, i) == reset[i]);
    rejected(&u, 0, 1, true, 'x'); /* Reset is not configured 8N1. */
    put(&u, 3, 0x80); put(&u, 0, 0x34); put(&u, 1, 0x12);
    assert(get(&u, 0)==0x34 && get(&u, 1)==0x12);
    put(&u, 3, 3); assert(!get(&u, 0) && !get(&u, 1));
    for (unsigned data=0; data<256; ++data) {
        uint32_t value=data; bool emit=false; uint8_t byte=0xa5;
        struct guest_uart saved=u;
        assert(guest_uart_access(&u, 0, 1, true, &value, &emit, &byte));
        assert(emit && byte==data && value==data && !memcmp(&u,&saved,sizeof u));
        assert(get(&u,5)==0x60 && get(&u,2)==1);
        put(&u, 7, (uint8_t)data); assert(get(&u,7)==data);
    }
    put(&u,3,0x83); assert(get(&u,0)==0x34 && get(&u,1)==0x12);
    put(&u,0,0); put(&u,1,0xff); assert(!get(&u,0) && get(&u,1)==0xff);
    put(&u,3,3);
    for (unsigned data=0; data<256; ++data) {
        if (data) rejected(&u,1,1,true,data); else put(&u,1,0);
        if (data&0x38) rejected(&u,2,1,true,data);
        else { put(&u,2,(uint8_t)data); assert(u.fcr==(data&0xc1)); assert(get(&u,2)==((data&1)?0xc1:1)); }
        if (data!=0 && data!=3 && data!=0x80 && data!=0x83) rejected(&u,3,1,true,data);
        else { put(&u,3,(uint8_t)data); assert(get(&u,3)==data); put(&u,3,3); }
        if (data&0xf0) rejected(&u,4,1,true,data);
        else { put(&u,4,(uint8_t)data); assert(get(&u,4)==data); assert(!get(&u,6)); }
        rejected(&u,5,1,true,data); rejected(&u,6,1,true,data);
    }
    for (unsigned i=0;i<8;++i) {
        rejected(&u,i,0,false,0); rejected(&u,i,2,false,0);
        rejected(&u,i,4,true,0); rejected(&u,i,UINT_MAX,true,0);
        rejected(&u,i,1,true,256); rejected(&u,i,1,true,UINT32_MAX);
    }
    rejected(&u,8,1,false,0); rejected(&u,UINT_MAX,1,true,0);
    uint32_t v=42; bool emit=true; uint8_t byte=0xa5; struct guest_uart saved=u;
    assert(!guest_uart_access(0,0,1,false,&v,&emit,&byte));
    assert(!guest_uart_access(&u,0,1,false,0,&emit,&byte));
    assert(!guest_uart_access(&u,0,1,false,&v,0,&byte));
    assert(!guest_uart_access(&u,0,1,false,&v,&emit,0));
    assert(v==42 && emit && byte==0xa5 && !memcmp(&u,&saved,sizeof u));
    u.lcr=7; rejected(&u,0,1,false,0); u=saved;
    u.mcr=0x10; rejected(&u,0,1,false,0); u=saved;
    u.fcr=2; rejected(&u,0,1,false,0);
    puts("guest UART: PASS (polling TX, DLAB, all 256 register values, atomic rejection)");
    return 0;
}
