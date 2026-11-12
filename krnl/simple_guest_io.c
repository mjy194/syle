#include "simple_guest.h"
#include "guest_uart.h"
/* Qualification permits only direction and the 16-bit port. Size must encode
 * one byte (0), string/REP must be clear, encoding must be DX (0), and every
 * reserved bit must be zero. No path performs physical port I/O. */
enum sg_action sg_dispatch_io(struct sg_run_state *s,const struct sg_image *image,
    struct guest_uart *uart,uint64_t q,uint64_t rip,uint64_t length,
    struct vmx_guest_registers *r,uint64_t *next,uint8_t *character) {
    if(!s) return SG_REJECT;
    if(s->failed || s->finished) return SG_REJECT;
    unsigned port=(unsigned)((q>>16)&0xffff);
    if(!image || !uart || !r || !next || !character ||
       s->exits>=SG_EXIT_LIMIT || (q&~UINT64_C(0xffff0008)) ||
       port<0x3f8 || port>0x3ff || (uint16_t)r->rdx!=port || length!=1 ||
       rip==UINT64_MAX || !sg_image_executable(image,rip,length)) {
        s->failed=true;return SG_REJECT;
    }
    bool write=(q&8)==0,emit=false;
    uint32_t value=(uint8_t)r->rax;
    uint8_t byte=0;
    struct guest_uart candidate=*uart;
    if(!guest_uart_access(&candidate,port-0x3f8,1,write,&value,&emit,&byte) ||
       (emit && (!write || s->characters>=SG_EXIT_LIMIT))) {
        s->failed=true;return SG_REJECT;
    }
    *uart=candidate;
    if(!write) r->rax=(r->rax&~UINT64_C(0xff))|(uint8_t)value;
    ++s->exits;*next=rip+1;
    if(emit) {++s->characters;*character=byte;return SG_PUTCHAR;}
    return SG_RESUME;
}
