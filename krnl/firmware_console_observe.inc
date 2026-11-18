#ifdef SYLE_FIRMWARE_CONSOLE_PROBE
static bool console_probe_complete;
static unsigned console_matches(const char*word){
 unsigned n=0;while(word[n])n++;unsigned count=0;
 for(unsigned i=0;i+n<=console_count;i++){unsigned j=0;while(j<n&&console_output[i+j]==(uint8_t)word[j])j++;if(j==n)count++;}
 return count;
}
#endif
static bool console_dispatch(uint64_t rip,uint64_t q,unsigned len){
 uint8_t b[15];size_t available=io_instruction_fetch(rip,b);bool emitted=false;uint8_t byte=0;
 if(!irq_identity()||!observation_ok())return false;
 if(!fcon_io(&console_state,native.gpr,rip,q,len,b,available,0,pic_write,&emitted,&byte))return false;
 if(emitted){
  if(console_count>=sizeof console_output)return false;
  console_output[console_count++]=byte;console_live_byte(byte);
#ifdef SYLE_FIRMWARE_CONSOLE_PROBE
  if(console_state.received>=32 && console_matches("\nSYLE_CONSOLE_OK_20261117\r")>=1 && console_matches("Shell>")>=3)console_probe_complete=true;
#endif
 }
 return true;
}
static bool console_config(uint64_t rip,uint64_t q,unsigned len){
 uint8_t b[15];unsigned available=(unsigned)io_instruction_fetch(rip,b);
 if(!fpp_encoding(q,len,b,available)||((q&64)==0&&(uint16_t)native.gpr[3]!=(uint16_t)(q>>16)))return false;
 unsigned size=q&7,width=size==3?4:size==1?2:1;
 struct fcon_state next=console_state;uint64_t value=native.gpr[0];
 unsigned off=(pci.pci.address&252)+((q>>16)&3);
 if(!fcon_config(&next,off,width,(q&8)!=0,&value))return false;
 if(!pic_write(0,0x681e,rip+len)){console_state.failed=true;return false;}
 console_state=next;native.gpr[0]=value;return true;
}
static bool console_poll(void){
 if(!console_state.command||console_state.failed||console_state.uart.rc||console_state.uart.mcr&16)return !console_state.failed;
 uint8_t status;__asm__ volatile("inb %1,%0":"=a"(status):"Nd"((uint16_t)0x3fd));
 if(status==255||status&0x1e)return false;
 if(status&1){uint8_t b;__asm__ volatile("inb %1,%0":"=a"(b):"Nd"((uint16_t)0x3f8));return fcon_receive(&console_state,b);}
 return true;
}
