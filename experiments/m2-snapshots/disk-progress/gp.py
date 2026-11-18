def patch(s,once):
 s=once(s,' if(reason==14)return disk_invlpg', ' if(reason==31&&(uint32_t)native.gpr[2]==0xce)return disk_inject_gp(rip,(unsigned)len);\n if(reason==14)return disk_invlpg')
 s=once(s,'  if(disk_pic_active){\n   if(!disk_tick()||!disk_pic_before', '  if(disk_gp_pending()){if(!disk_gp_entry())break;}else\n  if(disk_pic_active){\n   if(!disk_tick()||!disk_pic_before')
 s=once(s,'  if(disk_pic_active){\n   if(!disk_pic_after', '  if(disk_gp_pending()){\n   if(!disk_gp_after(native.entry_status==1&&result==LVS_EXIT&&exit_fields&&safe(),(uint32_t)raw_reason,(uint32_t)raw_vector,(uint32_t)raw_interrupt))break;\n  }else\n  if(disk_pic_active){\n   if(!disk_pic_after')
 return s
