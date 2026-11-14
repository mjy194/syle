/* Host-only composition experiment. Not a production entry wrapper. */
#include "../linux_guest_irq_glue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct guest_apic a;static struct ioapic io;static struct gia owner;
static struct lig_state tx;static unsigned calls,fail_at;
static bool wr(void *p,uint32_t f,uint64_t v) {
 (void)p;(void)f;(void)v;return ++calls!=fail_at;
}
static void setup(void) {
 guest_apic_init(&a,0);ioapic_init(&io,gia_enqueue,&owner);
 assert(gia_init(&owner,&a,&io,91,true));tx=(struct lig_state){0};calls=fail_at=0;
 struct guest_apic_effect e;
 assert(guest_apic_write(&a,0x80f,0x1ff,0,&e));
 assert(guest_apic_write(&a,0x832,0x20045,0,&e));
 assert(guest_apic_write(&a,0x838,1,0,&e));
}
/* Stronger firmware admission than generic gia_poll: disabled software APIC
 * cannot silently consume this prototype's unmasked due event. Any false
 * forbids entry; lifecycle terminal latch belongs outside this experiment. */
static bool transfer(uint64_t now) {
 if(!gia_exclusive(&owner)) return false;
 if(a.timer.armed && now>=a.timer.deadline && !(a.timer.lvt&(1u<<16)) &&
    (!(a.irq.svr&0x100) || (a.timer.lvt&255)<32)) return false;
 return gia_poll(&owner,now);
}
static bool pending(void) {return (a.irq.irr[2]&(1u<<5))!=0;}
int main(void) {
 struct lei_snapshot ready={.rflags=0x202};
 setup();uint64_t due=a.timer.deadline;assert(transfer(due*3));
 assert(pending() && a.timer.last_expirations==3 && a.timer.deadline==due*4);
 /* Re-polling the same sample cannot create another event or alter phase. */
 assert(transfer(due*3) && pending() && a.timer.deadline==due*4);
 ready.rflags=2;assert(lgig_before(&tx,&owner,&ready,0,wr));
 assert(!owner.reserved && !calls && pending());
 ready.rflags=0x202;assert(lgig_before(&tx,&owner,&ready,0,wr));
 assert(owner.reserved && owner.source==GIA_LOCAL_EDGE && pending());
 struct guest_apic saved=a;
 assert(!transfer(due*4) && !memcmp(&saved,&a,sizeof a));
 assert(lgig_after(&tx,&owner,LIG_NO_ENTRY,0,0,0,wr));
 assert(!owner.reserved && pending());
 /* Explicit new start after proven no-entry/cleanup, not implicit reentry. */
 tx=(struct lig_state){0};assert(lgig_before(&tx,&owner,&ready,0,wr));
 assert(lgig_after(&tx,&owner,LIG_EXIT,10,0,0,wr));
 assert(!pending() && (a.irq.isr[2]&(1u<<5)) && !owner.reserved);
 assert(transfer(due*4) && pending()); /* one new edge while ISR exists */
 setup();due=a.timer.deadline;a.irq.svr&=~0x100u;saved=a;
 assert(!transfer(due) && !memcmp(&saved,&a,sizeof a));
 /* VMCS staging failures must retain IRR/reservation and permanently bar entry. */
 for(unsigned f=1;f<=3;f++) {
  setup();assert(transfer(a.timer.deadline));fail_at=f;
  bool ok=lgig_before(&tx,&owner,&ready,0,wr);
  assert(!ok && tx.failed && pending() && owner.reserved);assert(!lgig_before(&tx,&owner,&ready,0,wr));
 }
 /* Unknown outcome is never converted to a retryable no-entry result. */
 setup();assert(transfer(a.timer.deadline));assert(lgig_before(&tx,&owner,&ready,0,wr));
 assert(!lgig_after(&tx,&owner,LIG_UNKNOWN,0,0,0,wr));
 assert(tx.failed && owner.reserved && pending());
 puts("firmware_timer_irq_composition: PASS (host only; legal vectors only)");
}
