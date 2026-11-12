#define main old_regression
#include "linux_guest_dispatch.c"
#undef main
static bool io(unsigned port,bool input,bool immediate){mock.f[0x6400]=((uint64_t)port<<16)|(input?8:0)|(immediate?64:0);mock.f[0x440c]=immediate?2:1;session.native.gpr[3]=port;return run(30);}
static void out(unsigned p,unsigned v){session.native.gpr[0]=v;assert(io(p,false,true));}
int main(void){setup(false);out(0x21,255);out(0x20,0x11);assert(dispatch.pic.master==0&&dispatch.pic.chip[0].phase==1);assert(io(0x21,true,true)&&!(uint8_t)session.native.gpr[0]);out(0x21,0x30);out(0x21,4);out(0x21,3);assert(dispatch.pic.chip[0].aeoi);
 out(0xa0,0x11);out(0xa1,0x38);out(0xa1,2);out(0xa1,1);assert(!dispatch.pic.chip[1].aeoi);assert(!lpic_entry_ok(&dispatch.pic,512));out(0x21,255);out(0xa1,255);assert(lpic_entry_ok(&dispatch.pic,512));out(0x21,251);assert(io(0x21,true,true)&&(uint8_t)session.native.gpr[0]==251);
 setup(false);mock.f[0x6820]=512;session.native.gpr[0]=0x11;assert(!io(0x20,false,true)&&dispatch.pic.master==255);
 setup(false);out(0x20,0x11);mock.f[0x6820]=512;assert(!lgd_before_entry(&dispatch,&session)&&dispatch.failed);
 for(unsigned step=0;step<4;step++){setup(false);if(step)out(0x20,0x11);if(step>1)out(0x21,0x30);if(step>2)out(0x21,4);struct lpic_state prior=dispatch.pic;mock.failwrite=mock.writes+4;session.native.gpr[0]=step==0?0x11:step==1?0x30:step==2?4:1;assert(!io(step?0x21:0x20,false,true)&&dispatch.pic.master==prior.master&&dispatch.pic.chip[0].phase==prior.chip[0].phase);}
 const unsigned bad[]={0x1f,0x31,0};for(unsigned i=0;i<3;i++){setup(false);out(0x20,0x11);session.native.gpr[0]=bad[i];assert(!io(0x21,false,true));}
 setup(false);out(0x20,0x11);out(0x21,0x30);session.native.gpr[0]=2;assert(!io(0x21,false,true));
 setup(false);out(0x20,0x11);out(0x21,0x30);out(0x21,4);session.native.gpr[0]=9;assert(!io(0x21,false,true));
 setup(false);out(0x20,0x11);out(0x21,0x30);out(0x20,0x11);assert(dispatch.pic.chip[0].phase==1&&!dispatch.pic.chip[0].vector&&!dispatch.pic.chip[0].aeoi);
 setup(false);out(0x20,0x11);out(0xa0,0x11);out(0x21,0x40);out(0xa1,0x48);out(0x21,4);out(0xa1,2);out(0x21,1);out(0xa1,3);assert(dispatch.pic.chip[1].aeoi);out(0xa0,0x11);assert(!dispatch.pic.chip[1].aeoi&&!dispatch.pic.chip[1].cascade&&!dispatch.pic.chip[1].vector);
 setup(false);out(0x20,0x11);out(0x21,0x30);out(0x21,4);out(0x21,1);mock.f[0x6820]=512;assert(!lgd_before_entry(&dispatch,&session));
 setup(false);out(0x20,0x11);out(0x21,0x30);out(0x21,4);out(0x21,3);session.prepared=false;assert(lgd_reset(&dispatch)&&!dispatch.pic.chip[0].started&&!dispatch.pic.chip[0].aeoi&&dispatch.pic.master==255);
 setup(false);out(0xa0,0x11);dispatch.pic.master=0;assert(!lpic_entry_ok(&dispatch.pic,0));
 setup(false);out(0x20,0x11);dispatch.pic.slave=0;assert(!lpic_entry_ok(&dispatch.pic,0));
 setup(false);out(0x20,0x11);dispatch.pic.chip[0].vector=0x30;assert(!lpic_entry_ok(&dispatch.pic,0));
 setup(false);out(0x20,0x11);out(0x21,0x30);dispatch.pic.chip[0].cascade=4;assert(!lpic_entry_ok(&dispatch.pic,0));
 setup(false);out(0x20,0x11);out(0x21,0x30);out(0x21,4);dispatch.pic.chip[0].aeoi=true;assert(!lpic_entry_ok(&dispatch.pic,0));
 puts("linux_pic_init: actual ICW stages/masks/vector/cascade/AEOI, IF0 transition gating, partial failures PASS (host only, NO IRQ delivery)");return 0;}
