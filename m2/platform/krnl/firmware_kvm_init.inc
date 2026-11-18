#include "nested_vmx.h"

/* Virtual control MSRs for the single-vCPU initialization profile. No MSR
 * number or VMX pointer supplied by L1 is used on the physical processor. */
static bool nested_capability(uint32_t msr,uint64_t *value){
 const uint32_t primary=(1u<<2)|(1u<<3)|(1u<<7)|(1u<<9)|(1u<<10)|(1u<<11)|
   (1u<<15)|(1u<<16)|(1u<<19)|(1u<<20)|(1u<<23)|(1u<<24)|(1u<<29)|(1u<<31);
 switch(msr){
 case 0x480:*value=NVMX_REVISION|(UINT64_C(4096)<<32)|(UINT64_C(6)<<50);break;
 case 0x481:*value=UINT64_C(9)<<32;break;
 case 0x482:*value=(uint64_t)primary<<32;break;
 case 0x483:*value=UINT64_C(0x8204)<<32;break;
 case 0x484:*value=UINT64_C(0x204)<<32;break;
 case 0x491: /* VM functions */
 case 0x492: /* tertiary controls */
 case 0x485:*value=0;break;
 case 0x486:*value=0x80000021;break;
 case 0x487:*value=0xe005003f;break;
 case 0x488:*value=0x2000;break;
 case 0x489:*value=0x26f8;break;
 case 0x48a:*value=0x2e;break;
 case 0x48b:*value=UINT64_C(2)<<32;break;
 case 0x48c:*value=(1u<<6)|(1u<<14)|(1u<<20)|(1u<<25)|(1u<<26);break;
 default:return false;
 }
 return true;
}

#include "nested_operand.h"
#include "nested_local_invept.h"
#include "firmware_nested_entry.inc"


/* Opaque VMCS12 data after Intel's revision/abort header. A cleared
 * region can outlive the active slot. No host pointer or launch flag is
 * serialized. Linux also allocates fresh, zeroed regions for new vCPUs. */
#define CLEARED_VMCS12_TAG UINT64_C(0x53594c45434c5201)
_Static_assert(16+sizeof nested_vmcs.fields<=4096,"cleared VMCS12 fits one page");
static bool nested_bind_region(uint64_t page,bool clear){
 uint64_t tag,fields[NVMX_FIELDS];
 if(!fwm_read_ram(&mem,page+8,&tag,sizeof tag))return false;
 if(tag!=CLEARED_VMCS12_TAG)return clear&&nvmx_bind_cleared(&nested_vmcs,page,NULL);
 return fwm_read_ram(&mem,page+16,fields,sizeof fields)&&nvmx_bind_cleared(&nested_vmcs,page,fields);
}
static bool nested_save_cleared(uint64_t page){
 const uint64_t tag=CLEARED_VMCS12_TAG;
 return fwm_write_ram(&mem,page+16,nested_vmcs.fields,sizeof nested_vmcs.fields)&&
        fwm_write_ram(&mem,page+8,&tag,sizeof tag);
}

static bool nested_root_instruction(uint64_t reason,uint64_t rip,uint64_t len,
                                    uint64_t qualification,uint64_t flags){
 irq0_cost.vmx_ops++;
 uint8_t bytes[15];uint64_t regs[16];
 struct nested_operand op;
 if(!(mode.cr4&0x2000)||!mode.ia32e||!disk_pic_active)return false;
 /* Register VMREAD/VMWRITE exits already identify both operands in
  * hardware. Memory forms still need translation and an address check. */
 uint64_t info=0;
 if((reason==23||reason==25)&&!rd(0x440e,&info))return false;
 if((reason==23||reason==25)&&(info&(1u<<10))&&(len==3||len==4)){
  uint64_t ar,base;
  if(!rd(0x4816,&ar)||(ar&~UINT64_C(1))!=0xa09a||!rd(0x6808,&base)||base||
     rip>UINT64_MAX-len||((rip+len)>>47!=0&&(rip+len)>>47!=0x1ffff))return false;
  op=(struct nested_operand){.op=reason==23?NVMX_READ:NVMX_WRITE,
     .regA=(info>>3)&15,.regB=(info>>28)&15};
  if(!reg_read(op.regB,&regs[op.regB])||!reg_read(op.regA,&regs[op.regA]))return false;
 }else{
  for(unsigned i=0;i<16;i++)if(!reg_read(i,&regs[i]))return false;
  if(!nested_decode_operand(bytes,io_instruction_fetch(rip,bytes),rip,regs,reason,&op)||
     op.length!=len)return false;
 }
 struct nvmx_request request={.op=op.op,.flags=flags};
 uint64_t gpa=0;
 struct nvmx_result result;
 if(op.memory){
  uint64_t info;
  struct fmm_memory m={.memory=&mem,.cr0=mode.cr0,.cr3=mode.cr3,
    .cr4=mode.cr4,.efer=mode.efer};
  uint64_t ar,cs;
  if(!rd(0x4816,&ar)||!rd(0x0802,&cs)||!rd(0x6808,&m.cs_base))return false;
  m.cs_ar=ar;m.cs_selector=cs;
  if(!rd(0x440e,&info)||!nested_memory_exit_matches(info,qualification,regs,op.ea)||
     !fmm_translate(&m,op.ea,op.op==NVMX_PTRST?FMM_WRITE:FMM_READ,&gpa))return false;
  if(op.op==NVMX_INVEPT){
   uint64_t descriptor[2];
   if(!fwm_read_ram(&mem,gpa,descriptor,sizeof descriptor))return false;
   uint64_t type=regs[op.regB];
   enum nested_local_invept_status status=nested_local_invept_check_allcontexts(type,descriptor[0],descriptor[1],32);
   if(status==NESTED_LOCAL_INVEPT_UNSUPPORTED)return false;
   result=nvmx_invept_result(&nested_vmcs,status==NESTED_LOCAL_INVEPT_OK,flags);
  }else if(op.op!=NVMX_PTRST){
   if(!fwm_read_ram(&mem,gpa,&request.operand,8))return false;
   if(!(request.operand&4095)&&request.operand<FWM_RAM_SIZE){
    if(!fwm_read_ram(&mem,request.operand,&request.region_revision,4))return false;
    if(op.op==NVMX_ON&&!nested_vmcs.root)nested_vmcs.on_region=request.operand;
    if((op.op==NVMX_CLEAR||op.op==NVMX_PTRLD)&&nested_vmcs.root&&!nested_vmcs.active&&
       request.operand!=nested_vmcs.on_region&&
       !nested_bind_region(request.operand,op.op==NVMX_CLEAR))return false;
   }else if(op.op!=NVMX_ON)return false;
  }
 }
 if(op.op==NVMX_WRITE||op.op==NVMX_READ){
  request.field=regs[op.regB];
  request.value=regs[op.regA];
  nested_last_field=request.field;
 }
 if(op.op!=NVMX_INVEPT)result=nvmx_execute(&nested_vmcs,request);
 if(result.status==NVMX_ENTRY_REQUEST)return nested_run_entry();
 if(result.status!=NVMX_SUCCESS&&result.status!=NVMX_FAIL_VALID&&
    result.status!=NVMX_FAIL_INVALID)return false;
 if(result.status==NVMX_SUCCESS){
  if(op.op==NVMX_CLEAR&&!nested_save_cleared(request.operand))return false;
  if(op.op==NVMX_OFF||op.op==NVMX_CLEAR)nested_drop_cpu();
  if(op.op==NVMX_INVEPT||op.op==NVMX_OFF||op.op==NVMX_CLEAR)nested_drop_ept();
  if(op.op==NVMX_PTRST&&!fwm_write_ram(&mem,gpa,&result.value,8))return false;
  if(op.op==NVMX_READ&&!reg_write(op.regA,result.value))return false;
 }
 return wr(0x6820,result.flags)&&wr(0x681e,rip+len);
}
