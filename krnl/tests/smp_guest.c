#include "../smp_guest.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    struct smp_guest_context c={.host_restored=true,.exit_reason=18,.exit_rip=0x6010,.vmcall_gpa=0x6010,
        .exit_length=3,.exit_rsp=0xa000,.stack_top=0xa000,.expected_data=0x1122,.owner_token=0xb500,
        .frame={.rax=0x1122,.r8=0xb500,.r9=0xb500}};
    assert(smp_guest_exit_valid(&c));
#define BAD(field,value) do{struct smp_guest_context x=c;x.field=value;assert(!smp_guest_exit_valid(&x));}while(0)
    BAD(exit_reason,0x80000021);BAD(exit_reason,48);BAD(exit_rip,0x6011);BAD(exit_length,0);
    BAD(exit_rsp,0xb000);BAD(frame.rax,0);BAD(frame.r8,0xa500);BAD(frame.r9,0xa500);BAD(host_restored,false);
    assert(!smp_guest_exit_valid(0));
    assert(!smp_guest_resume_valid(&c));
    c.vmcs_live=true;c.launched=true;c.status=SMP_GUEST_OK;
    assert(smp_guest_resume_valid(&c));
    c.cleared=true;assert(!smp_guest_resume_valid(&c));c.cleared=false;
    c.status=SMP_GUEST_FLUSH_FAILED;assert(!smp_guest_resume_valid(&c));
    c.status=SMP_GUEST_OK;c.launched=false;assert(!smp_guest_resume_valid(&c));
    assert(!smp_guest_resume_valid(0));struct smp_guest_context k={.host_restored=true,.kick_mode=true,.exit_reason=1,
        .exit_info=0x80000041,.loop_start=0x6021,.loop_end=0x602a,.exit_rip=0x6021,
        .stack_top=0xb000,.exit_rsp=0xb000,.owner_token=12,.frame={.r8=12,.r9=12}};
    assert(smp_guest_kick_valid(&k));
#define K_BAD(field,value) do{struct smp_guest_context x=k;x.field=value;assert(!smp_guest_kick_valid(&x));}while(0)
    K_BAD(exit_reason,0x80000001);K_BAD(exit_info,0x41);K_BAD(exit_info,0x80000040);
    K_BAD(exit_info,0x80000341);K_BAD(exit_flags,0x202);K_BAD(exit_rip,0x602a);
    K_BAD(exit_rip,0x6020);K_BAD(exit_rsp,0xa000);K_BAD(host_restored,false);
    K_BAD(kick_mode,false);K_BAD(frame.r9,13);assert(!smp_guest_kick_valid(0));
    puts("PASS SMP perCPU guest exit evidence rejection");
}
