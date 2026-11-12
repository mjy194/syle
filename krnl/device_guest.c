#include "device_guest.h"
#include "device_bus.h"
#include "ioapic.h"
#include "mmio_decode.h"
#include "lapic.h"
#include "paging.h"
#include "../efi/src/exceptions.h"
extern struct vmx_guest_registers vmx_guest_regs;
static struct device_bus bus;
static struct ioapic io;
static struct lapic local;
static bool injected,accepted,window;
static uint8_t selected;
static unsigned deliveries,mmios,stimulus;
extern const unsigned char ept_apic_first_isr[];
static bool get(uint64_t f,uint64_t *v) {
    unsigned char fail;
    __asm__ volatile("vmread %2,%1; setna %0":"=qm"(fail),"=r"(*v):"r"(f):"cc");
    return !fail;
}
static bool put(uint64_t f,uint64_t v) {
    unsigned char fail;
    __asm__ volatile("vmwrite %2,%1; setna %0":"=qm"(fail):"r"(f),"r"(v):"cc","memory");
    return !fail;
}
static bool enqueue(void *ctx,uint8_t vector) {
    return lapic_write(ctx,0x83f,vector);
}
static uint64_t *ram(struct ept_fixture *f,uint64_t gpa) {
    if(!device_guest_ram_gpa(gpa)) return 0;
    unsigned page=(unsigned)(gpa>>12);
    return paging_phys_to_virt(f->pages[page-1]);
}
/* Four-level supervisor, non-large mappings only; never follow host pointers
 * from guest bytes. Table backing must be one of this fixture's private pages. */
static bool walk(struct ept_fixture *f,uint64_t va,bool write,bool execute,uint64_t *pa) {
    uint64_t cr3,cr4,table=0x1000;
    if(!get(0x6802,&cr3)||cr3!=table||!get(0x6804,&cr4)||(cr4&(1u<<12))) return false;
    if(va>=UINT64_C(0x800000000000)) return false; /* low canonical fixture */
    for(int shift=39;shift>=12;shift-=9) {
        uint64_t *p=ram(f,table);
        if(!p||(table&0xfff)) return false;
        uint64_t entry=p[(va>>shift)&511];
        /* No PS, user, global, software/unsupported high bits in this profile. */
        if(!(entry&1)||(write&&!(entry&2))||(execute&&(entry>>63))||
           (entry&~UINT64_C(0x800ffffffffff063))) return false;
        table=entry&UINT64_C(0x000ffffffffff000);
    }
    *pa=table|(va&0xfff);return true;
}
static void regs_read(uint64_t r[16],uint64_t rsp) {
    r[0]=vmx_guest_regs.rax;r[1]=vmx_guest_regs.rcx;r[2]=vmx_guest_regs.rdx;
    r[3]=vmx_guest_regs.rbx;r[4]=rsp;r[5]=vmx_guest_regs.rbp;
    r[6]=vmx_guest_regs.rsi;r[7]=vmx_guest_regs.rdi;
    r[8]=vmx_guest_regs.r8;r[9]=vmx_guest_regs.r9;
    r[10]=vmx_guest_regs.r10;r[11]=vmx_guest_regs.r11;
    r[12]=vmx_guest_regs.r12;r[13]=vmx_guest_regs.r13;
    r[14]=vmx_guest_regs.r14;r[15]=vmx_guest_regs.r15;
}
static bool reg_write(unsigned n,uint32_t v) {
    uint64_t *p[]={&vmx_guest_regs.rax,&vmx_guest_regs.rcx,&vmx_guest_regs.rdx,
        &vmx_guest_regs.rbx,0,&vmx_guest_regs.rbp,&vmx_guest_regs.rsi,&vmx_guest_regs.rdi,
        &vmx_guest_regs.r8,&vmx_guest_regs.r9,&vmx_guest_regs.r10,&vmx_guest_regs.r11,
        &vmx_guest_regs.r12,&vmx_guest_regs.r13,&vmx_guest_regs.r14,&vmx_guest_regs.r15};
    if(n==4) return put(0x681c,v);
    *p[n]=v;return true;
}
static bool mmio(struct ept_fixture *f,uint64_t rip,uint64_t rsp) {
    uint64_t codepa,pa,q,gla,gpa,r[16],blocking,flags,debug;struct mmio_mov32 op;
    /* No shadow consumption or single-step emulation in this subset. */
    if(!get(0x4824,&blocking)||!get(0x6820,&flags)||!get(0x6822,&debug)||
       !device_mmio_state_supported(blocking,flags,debug)) return false;
    if(!get(0x6400,&q)||!get(0x640a,&gla)||!get(0x2400,&gpa)||
       !walk(f,rip,false,true,&codepa)||codepa<0x6000||codepa>=0x7000) return false;
    unsigned offset=codepa&4095;
    const uint8_t *code=(const uint8_t *)ram(f,codepa)+offset;
    regs_read(r,rsp);
    if(!mmio_decode_mov32(code,4096-offset,rip,r,&op)||
       (op.ea&3)||op.ea!=gla||!walk(f,op.ea,op.write,false,&pa)||pa!=gpa||
       (q&0x1ff)!=(op.write?0x182u:0x181u)||rip>UINT64_MAX-op.length) return false;
    uint32_t value=(uint32_t)r[op.reg];
    if(device_bus_access(&bus,pa,4,op.write,&value)!=DEVICE_HANDLED) return false;
    /* No side effect before all decode/translation checks. After successful
     * device transaction any failed VMWRITE stops; never replay a store. */
    if(!op.write&&!reg_write(op.reg,value)) return false;
    if(!put(0x681e,rip+op.length)) return false;
    mmios++;log_text("DEVICE.MMIO_HANDLED RIP=");log_hex(rip);
    log_text("DEVICE.MMIO_GPA=");log_hex(gpa);return true;
}
bool device_guest_init(void) {
    lapic_init(&local);device_bus_init(&bus);ioapic_init(&io,enqueue,&local);
    injected=accepted=window=false;deliveries=mmios=stimulus=0;
    return device_bus_register(&bus,0xe000,0x1000,ioapic_mmio,&io);
}
static int arm(uint32_t primary) {
    uint8_t vector;
    if(injected||window||!lapic_select(&local,&vector)) return 0;
    if(!put(0x4002,primary|4)) return -1;
    window=true;return 0;
}
int device_guest_exit(struct ept_fixture *f,uint32_t primary,uint64_t reason,
                      uint64_t rip,uint64_t rsp) {
    uint64_t vectoring,pending,flags,blocking,activity,length;
    if(!get(0x4408,&vectoring)||(vectoring&(1u<<31))) return -1;
    if(injected&&!accepted) {
        uint64_t first=EPT_CODE_GPA+(uintptr_t)ept_apic_first_isr-(uintptr_t)ept_payload_start;
        if(!lapic_irq_ack_valid(reason,rip,first,vectoring)||
           !lapic_accept(&local,selected)||!put(0x4016,0)) return -1;
        accepted=true;deliveries++;
        log_text("DEVICE.IRQ_ACCEPT_COUNT=");log_hex(deliveries);
    }
    if(reason==7) {
        if(!window||injected||!get(0x6820,&flags)||!get(0x4824,&blocking)||
           !get(0x4826,&activity)||!get(0x4016,&pending)||
           !lapic_window_ready(&local,reason,flags,blocking,activity,pending,vectoring,&selected)) return -1;
        if(!put(0x4002,primary)||!put(0x4018,0)||!put(0x401a,0)||
           !put(0x4016,0x80000000u|selected)) return -1;
        window=false;injected=true;accepted=false;return 0;
    }
    if(reason==48) {
        if(!mmio(f,rip,rsp)) return -1;
        return arm(primary);
    }
    if(!get(0x440c,&length)||rip>UINT64_MAX-length) return -1;
    if((reason==31||reason==32)&&length==2) {
        uint32_t index=(uint32_t)vmx_guest_regs.rcx;
        uint64_t value=((uint64_t)(uint32_t)vmx_guest_regs.rdx<<32)|(uint32_t)vmx_guest_regs.rax;
        if(reason==31) {
            if(!lapic_read(&local,index,&value)) return -1;
            vmx_guest_regs.rax=(uint32_t)value;vmx_guest_regs.rdx=(uint32_t)(value>>32);
        } else if(index==0x80b) {
            /* EOI is routed by the actual highest ISR, not a hardcoded vector. */
            int highest=-1;
            for(int i=7;i>=0&&highest<0;i--) {
                uint64_t bits;if(!lapic_read(&local,0x810+i,&bits)) return -1;
                for(int j=31;j>=0;j--) if(bits&(UINT64_C(1)<<j)) {highest=i*32+j;break;}
            }
            if(!accepted||highest<0||value||!lapic_write(&local,index,value)||
               !ioapic_eoi(&io,(uint8_t)highest)) return -1;
            injected=accepted=false;log_text("DEVICE.EOI_VECTOR=");log_hex(highest);
        } else if(index==0x83f||!lapic_write(&local,index,value)) return -1;
    } else if(reason==18&&length==3) {
        uint64_t command=vmx_guest_regs.rax;
        if(command==0x100&&stimulus==0) {
            if(!ioapic_set_irq(&io,0,true)||!ioapic_set_irq(&io,0,false)) return -1;
            stimulus=1;
        } else if(command==0x200&&stimulus==1&&deliveries==1) {
            if(!ioapic_set_irq(&io,1,true)) return -1;
            stimulus=2;
        } else if(command==0x300&&stimulus==2&&deliveries==3) {
            if(!ioapic_set_irq(&io,1,false)) return -1;
            stimulus=3;
        } else if(command==0x400&&stimulus==3&&deliveries==3&&vmx_guest_regs.r15==3) {
            uint64_t irr,isr;
            if(injected||window||mmios<10||!lapic_read(&local,0x822,&irr)||irr||
               !lapic_read(&local,0x812,&isr)||isr) return -1;
            log_text("DEVICE.IOAPIC_PASS edge=1 level=2 mmio=real deassert_stopped=1\n");return 1;
        } else return -1;
    } else return -1;
    if(!put(0x681e,rip+length)) return -1;
    return arm(primary);
}
