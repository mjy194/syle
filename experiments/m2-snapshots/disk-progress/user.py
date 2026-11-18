def patch(s,once):
 s=once(s,'static bool disk_tick(void);','static bool disk_tick(void);\nstatic bool disk_cs_identity(uint64_t,uint64_t);')
 s=once(s,'rd(0x0802,&cs)&&cs&&!(cs&7)&&\n rd(0x4816,&cs_ar)&&(cs_ar&~UINT64_C(1))==0xa09a', 'rd(0x0802,&cs)&&cs&&cs<=UINT16_MAX&&\n rd(0x4816,&cs_ar)&&disk_cs_identity(cs,cs_ar)')
 s=once(s,' if(cs&3){log_text("OVMF.CLOCK_APIC reject=cpl cs=");log_hex(cs);log_text("\\n");return false;}', ''' if(cs&3){
  uint64_t ar;if((reason!=10&&reason!=16&&reason!=52)||!rd(0x4816,&ar)||!disk_cs_identity(cs,ar))return false;
 }''')
 s=once(s,'bool resume=fpr_resume(reason,cs,flags,blocking,pending_debug,dr7,event,vector);', '''/* For CPL3, identity above authenticated the flat user CS/SS. The pure
   * preemption flag/event predicate is privilege-independent. */
  bool resume=fboot_preemption_resume(reason,cs,flags,blocking,pending_debug,dr7,event,vector);''')
 return s

def guard(s,once):
 s=once(s,'rd(0x4816,&cs_ar)&&disk_cs_identity(cs,cs_ar)', '''rd(0x4816,&cs_ar)&&
#ifdef SYLE_FIRMWARE_DISK
 disk_cs_identity(cs,cs_ar)
#else
 !(cs&7)&&(cs_ar&~UINT64_C(1))==0xa09a
#endif
''')
 s=once(s,''' if(cs&3){
  uint64_t ar;if((reason!=10&&reason!=16&&reason!=52)||!rd(0x4816,&ar)||!disk_cs_identity(cs,ar))return false;
 }''', ''' if(cs&3){
#ifdef SYLE_FIRMWARE_DISK
  uint64_t ar;if((reason!=10&&reason!=16&&reason!=52)||!rd(0x4816,&ar)||!disk_cs_identity(cs,ar))return false;
#else
  log_text("OVMF.CLOCK_APIC reject=cpl cs=");log_hex(cs);log_text("\\n");return false;
#endif
 }''')
 s=once(s,'bool resume=fboot_preemption_resume(reason,cs,flags,blocking,pending_debug,dr7,event,vector);', '''bool resume;
#ifdef SYLE_FIRMWARE_DISK
  resume=fboot_preemption_resume(reason,cs,flags,blocking,pending_debug,dr7,event,vector);
#else
  resume=fpr_resume(reason,cs,flags,blocking,pending_debug,dr7,event,vector);
#endif''')
 return s
