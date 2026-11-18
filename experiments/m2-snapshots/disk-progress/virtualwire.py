"""Explicit PIC ExtINT ownership after a quiescent firmware timer handoff."""
def patch(s,once):
 s=once(s,'static struct ftis_session irq_session;','static bool disk_pic_active,disk_pic_window;\nstatic bool disk_tick(void);\nstatic struct ftis_session irq_session;')
 s=once(s,'bitmap<=UINT32_MAX;', 'bitmap<=UINT32_MAX&&(!disk_pic_active||bitmap==0x42);')
 s=once(s,'primary==(expected_primary|(irq_window.armed?4:0))','primary==(expected_primary|((irq_window.armed||disk_pic_window)?4:0))')
 s=once(s,'static uint8_t console_output[16384]','static uint8_t console_output[65536]')
 s=once(s,'!lpic_entry_ok(&pic,flags))break;','!(disk_pic_active?disk_pic_entry_ok(flags):lpic_entry_ok(&pic,flags)))break;')
 s=once(s,'  struct lei_snapshot snapshot;uint32_t primary;bool priority;','  struct lei_snapshot snapshot;uint32_t primary;bool priority;') if False else s
 s=once(s,'  uint64_t irq_attempt=0,window_attempt=0;', '#ifdef SYLE_FIRMWARE_DISK\n  if(disk_pic_active&&!disk_input()){log_text("OVMF.DISK input_before_reservation_failed\\n");break;}\n#endif\n  uint64_t irq_attempt=0,window_attempt=0;')
 s=once(s,'  if(irq_session.ready){\n   struct lei_snapshot snapshot;', '  if(disk_pic_active){\n   if(!disk_tick()||!disk_pic_before(&staged))break;\n  }else if(irq_session.ready){\n   struct lei_snapshot snapshot;')
 s=once(s,'  if(irq_session.ready){\n   uint64_t entry_info,error,length;','  if(!disk_pic_active&&irq_session.ready){\n   uint64_t entry_info,error,length;')
 s=once(s,'  if(staged){\n   struct fine_outcome evidence=', '  if(disk_pic_active){\n   if(!disk_pic_after(native.entry_status==1&&result==LVS_EXIT&&exit_fields&&safe()&&irq_identity(),(uint32_t)raw_reason,(uint32_t)(raw_vector&0x80000000?raw_vector:0),&halt_receipt))break;\n  }else if(staged){\n   struct fine_outcome evidence=')
 s=once(s,'  if(!console_poll())','  if(!disk_pic_active&&!disk_input())')
 s=once(s,'  if(reason==7){\n   if(!windowed)break;', '  if(reason==7){\n   if(disk_pic_active){\n    if(!disk_pic_window||!gw_window_exit(0,disk_read_field,irq_write,disk_pic_run.must_one,disk_pic_run.allowed_one))break;\n    disk_pic_window=false;native.resume=1;continue;\n   }\n   if(!windowed)break;')
 s=once(s,'  if(port==0x61||port==0x42||', '''  if(port==0x80||(port>=0x3f8&&port<=0x3ff)||(port>=0x2f8&&port<=0x2ff)||(port>=0x3e8&&port<=0x3ef)||(port>=0x2e8&&port<=0x2ef))return disk_legacy_io(rip,q,(unsigned)len,flags);
  if(port==0x40||(port==0x43&&((uint8_t)native.gpr[0]>>6)!=2))return disk_legacy_io(rip,q,(unsigned)len,flags);
  if((port==0x20||port==0x21||port==0xa0||port==0xa1)&&(disk_pic_active||(!(q&8)&&port==0x20&&(uint8_t)native.gpr[0]==0x11&&irq_session.ready&&irq_session.owner.masked)))return disk_legacy_io(rip,q,(unsigned)len,flags);
  if(port==0x61||port==0x42||''')
 s=once(s,'if(!disk_bytes||!vfr_bind','if(!disk_legacy_init()||!disk_bytes||!vfr_bind')
 s=once(s,' log_text("OVMF.DISK terminal bar=");',' log_text("OVMF.DISK PIC receipts=");log_hex(disk_pic_receipts);log_text(" irq0=");log_hex(disk_irq0_receipts);log_text(" irq4=");log_hex(disk_irq4_receipts);log_text(" rx_sent=");log_hex(disk_rx_sent);log_text(" rx_read=");log_hex(disk_rx_read);log_text(" active=");log_hex(disk_pic_active);log_text(" IRR=");log_hex(disk_legacy.master.irr);log_text(" ISR=");log_hex(disk_legacy.master.isr);log_text(" mask=");log_hex(disk_legacy.master.imr);log_text("\\n");\n log_text("OVMF.DISK terminal bar=");')
 # Device state advances from the same sampled source at loop boundaries. No
 # synthetic exit-time or discrete-event jumps; halted VM keeps native timer.
 s=once(s,'  if(!observation_ok()||!clock_sync()||active_apic.failed', '  if(!observation_ok()||!clock_sync()||!disk_tick()||active_apic.failed')
 # Prove the actual port/instruction even for a stop outside dispatch.
 s=once(s,'OVMF.CLOCK_APIC failed reason=', 'OVMF.CLOCK_APIC failed reason=') if False else s
 return s

def guard(s,once):
 s=once(s,'static bool disk_pic_active,disk_pic_window;\nstatic bool disk_tick(void);', '''#ifdef SYLE_FIRMWARE_DISK
static bool disk_pic_active,disk_pic_window;
static bool disk_tick(void);
#else
#define disk_pic_active false
#define disk_pic_window false
#define disk_tick() true
#endif''')
 s=once(s,'  uint64_t flags;if(debug_bank.failed||!de_gate(&entry_debug)||!rd(0x6820,&flags)||(flags&0x100)||!(disk_pic_active?disk_pic_entry_ok(flags):lpic_entry_ok(&pic,flags)))break;', '''  uint64_t flags;if(debug_bank.failed||!de_gate(&entry_debug)||!rd(0x6820,&flags)||(flags&0x100))break;
#ifdef SYLE_FIRMWARE_DISK
  if(!(disk_pic_active?disk_pic_entry_ok(flags):lpic_entry_ok(&pic,flags)))break;
#else
  if(!lpic_entry_ok(&pic,flags))break;
#endif''')
 s=once(s,'  if(disk_pic_active){\n   if(!disk_tick()||!disk_pic_before(&staged))break;\n  }else if(irq_session.ready){', '''#ifdef SYLE_FIRMWARE_DISK
  if(disk_pic_active){
   if(!disk_tick()||!disk_pic_before(&staged))break;
  }else
#endif
  if(irq_session.ready){''')
 s=once(s,'  if(!disk_pic_active&&!disk_input()){log_text("OVMF.CONSOLE input_error\\n");break;}', '''#ifdef SYLE_FIRMWARE_DISK
  if(!disk_pic_active&&!disk_input()){log_text("OVMF.CONSOLE input_error\\n");break;}
#else
  if(!console_poll()){log_text("OVMF.CONSOLE input_error\\n");break;}
#endif''')
 s=once(s,'  if(disk_pic_active){\n   if(!disk_pic_after', '#ifdef SYLE_FIRMWARE_DISK\n  if(disk_pic_active){\n   if(!disk_pic_after')
 s=once(s,'&halt_receipt))break;\n  }else if(staged){','&halt_receipt))break;\n  }else\n#endif\n  if(staged){')
 s=once(s,'   if(disk_pic_active){\n    if(!disk_pic_window', '#ifdef SYLE_FIRMWARE_DISK\n   if(disk_pic_active){\n    if(!disk_pic_window')
 s=once(s,'    disk_pic_window=false;native.resume=1;continue;\n   }\n   if(!windowed)', '    disk_pic_window=false;native.resume=1;continue;\n   }\n#endif\n   if(!windowed)')
 return s
