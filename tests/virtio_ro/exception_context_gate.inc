/* Included only in isolated exceptions.c candidate, where IDT/GDT/TSS are
 * private actual objects. Refusal gate, NOT mapping lifetime/concurrency proof. */
extern unsigned exceptions_fatal_context(void);
extern void exception_2(void);
extern int paging_ap_start_attempted(void);
int exceptions_owned_context(void) {
    struct table_ptr current_idt,current_gdt;
    UINT16 cs,ss,tr;
    UINT64 flags,apic;
#ifdef ECG_HOST_TEST
    extern void ecg_test_capture(struct table_ptr *,struct table_ptr *,UINT16 *,UINT16 *,UINT16 *,UINT64 *,UINT64 *);
    ecg_test_capture(&current_idt,&current_gdt,&cs,&ss,&tr,&flags,&apic);
#else
    __asm__ volatile("mov %%cs,%0":"=r"(cs));
    if (cs & 3) return 0;
    __asm__ volatile("mov %%ss,%0":"=r"(ss));
    __asm__ volatile("pushfq; popq %0":"=r"(flags)::"memory");
    __asm__ volatile("sidt %0":"=m"(current_idt)::"memory");
    __asm__ volatile("sgdt %0":"=m"(current_gdt)::"memory");
    __asm__ volatile("str %0":"=r"(tr));
    UINT32 lo,hi;
    __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0x1bu):"memory");
    apic=((UINT64)hi<<32)|lo;
#endif
    if (cs!=8||ss!=16||(flags&((UINT64)0x600))||!(apic&((UINT64)1<<8))||
        paging_ap_start_attempted()||exceptions_fatal_context()||panicking) return 0;
    if (current_idt.base!=(UINTN)idt||current_idt.limit!=sizeof(idt)-1||
        current_gdt.base!=(UINTN)gdt||current_gdt.limit!=sizeof(gdt)-1||tr!=0x18) return 0;
    const struct gate nmi=idt[2];
    UINT64 target=(UINT64)nmi.offset_low|((UINT64)nmi.offset_mid<<16)|((UINT64)nmi.offset_high<<32);
    if (target!=(UINTN)exception_2||nmi.selector!=8||nmi.ist!=3||nmi.flags!=0x8e||nmi.reserved) return 0;
    UINT64 base=(UINTN)&tss,limit=sizeof(tss)-1;
    UINT64 expected=limit|((base&0xffffff)<<16)|((UINT64)0x8b<<40)|(((base>>24)&255)<<56);
    if (gdt[3]!=expected||gdt[4]!=base>>32||
        (gdt[1]&~((UINT64)1<<40))!=0x00af9a000000ffffULL||
        (gdt[2]&~((UINT64)1<<40))!=0x00cf92000000ffffULL||
        tss.ist[2]!=(UINTN)(nmi_stack+sizeof(nmi_stack))||tss.iomap!=sizeof(tss)) return 0;
    /* Recheck asynchronous refusal state, not a claim of an atomic snapshot.
     * On this BSP any NMI after admission must fatal without cleanup/return. */
    __asm__ volatile("" ::: "memory");
    return !exceptions_fatal_context()&&!panicking&&!paging_ap_start_attempted();
}
