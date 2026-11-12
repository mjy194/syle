#include "simple_guest.h"
#include "vmexit.h"
/* The only accepted exits are instruction exits with verified ELF spans.
 * Failure is sticky and never changes GPRs or caller output parameters. */
enum sg_action sg_dispatch(struct sg_run_state *s,const struct sg_image *image,
    uint64_t reason,uint64_t rip,uint64_t length,struct vmx_guest_registers *r,
    uint64_t *next,uint8_t *character) {
    if(!s) return SG_REJECT;
    if(s->failed || s->finished) return SG_REJECT;
    if(!image || !r || !next || !character || s->exits>=SG_EXIT_LIMIT ||
       !((reason==10 && length==2)||(reason==18 && length==3)) ||
       !sg_image_executable(image,rip,length) || rip>UINT64_MAX-length ||
       (reason==18 && r->rax!=1 && r->rax!=2) ||
       (reason==18 && r->rax==1 && s->characters>=SG_EXIT_LIMIT)) {
        s->failed=true;return SG_REJECT;
    }
    ++s->exits;
    if(reason==10) {vmexit_cpuid(r);*next=rip+length;return SG_RESUME;}
    if(r->rax==1) {++s->characters;*character=(uint8_t)r->rbx;*next=rip+length;return SG_PUTCHAR;}
    s->finished=true;s->exit_code=r->rbx;s->failed=r->rbx!=0;
    return SG_FINISH;
}
