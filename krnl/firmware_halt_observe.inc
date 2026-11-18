/* Opt-in adapter shared by the checkout and frozen runtime snapshot. */
static bool halt_read(void*x,uint32_t f,uint64_t*v){(void)x;return traced_rd(f,v);}
static bool halt_dispatch(void){
 struct fhl_trace t={0};
 if(!irq_session.ready||!irq_identity()||!observation_ok())return false;
 enum fat_result result=fhl_handle(&halt_state,&mem,true,halt_supported,0,halt_read,irq_write,&t);
 log_text("OVMF.HLT completion result=");log_hex((uint64_t)(int64_t)result);
 log_text(" rip=");log_hex(t.rip);log_text(" flags=");log_hex(t.flags);
 log_text(" blocking=");log_hex(t.blocking);log_text(" activity_before=");log_hex(t.activity);
 log_text(" opcode=");log_hex(t.opcode);log_text(" available=");log_hex(t.available);
 log_text(" resume_rip=");log_hex(halt_state.resume_rip);log_text(" halted=");log_hex(halt_state.halted);
 log_text(" completed=");log_hex(halt_state.completed);log_text(" failed=");log_hex(halt_state.failed);
 log_text(" reference=");log_hex(active_apic.reference);log_text(" pending=");log_hex(irq_session.owner.pending);
 log_text(" ISR=");log_hex(irq_session.owner.in_service);log_text(" deadline=");log_hex(irq_session.owner.deadline_bus);
 log_text(" no_synthetic_clock_or_IRQ\n");
 if(result==FAT_FATAL)mode_commit_failed();
 return result==FAT_COMMITTED;
}
static bool halt_entry_check(void){
 uint64_t activity,rip,flags,block;
 if(halt_state.failed)return false;
 if(!halt_state.halted)return true;
 return rd(0x4826,&activity)&&activity==1&&rd(0x681e,&rip)&&rip==halt_state.resume_rip&&
 rd(0x6820,&flags)&&flags==halt_state.halt_flags&&rd(0x4824,&block)&&!block;
}
static bool halt_after_entry(bool proven,bool staged,bool accounted){
 struct fhl_trace t={0};
 enum fat_result result=fhl_after(&halt_state,true,proven,staged,accounted,0,halt_read,irq_write,&t);
 log_text("OVMF.HLT after_entry result=");log_hex((uint64_t)(int64_t)result);
 log_text(" injected=");log_hex(staged);log_text(" receipt_verified=");log_hex(accounted);
 log_text(" reason=");log_hex(t.reason);log_text(" rip=");log_hex(t.rip);
 log_text(" activity_exit=");log_hex(t.activity);log_text(" halted=");log_hex(halt_state.halted);
 log_text(" idle_exits=");log_hex(halt_state.idle_exits);log_text(" wakeups=");log_hex(halt_state.wakeups);
 log_text(" failed=");log_hex(halt_state.failed);log_text("\n");
 if(result!=FAT_COMMITTED)mode_commit_failed();
 return result==FAT_COMMITTED;
}
