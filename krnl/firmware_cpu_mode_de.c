#include "firmware_cpu_mode_de.h"
bool fcmd_write(const struct fcm_state*s,uint64_t ram,enum fcm_register reg,
 uint64_t value,const struct fcmd_gate*g,struct fcm_state*out){
 if(!s||!g||!out||!(g->host_leaf1_edx&4)||g->primary_controls>>32||
 !(g->primary_controls&(UINT64_C(1)<<23))||g->debugctl||g->dr7!=0x400||g->pending_debug||
 g->entry_controls>>32||g->exit_controls>>32||
 !(g->entry_controls&4)||!(g->exit_controls&4))return false;
 struct fcm_state base=*s,next;
 uint64_t de=(reg==FCM_CR4?value:s->cr4)&8;
 base.cr4&=~UINT64_C(8);
 if(!fcm_write(&base,ram,reg,reg==FCM_CR4?value&~UINT64_C(8):value,&next))return false;
 next.cr4|=de;
 uint64_t hardware=next.cr4|UINT64_C(0x2000);
 if((hardware&g->cr4_fixed0)!=g->cr4_fixed0||(hardware&~g->cr4_fixed1))return false;
 *out=next;return true;
}
