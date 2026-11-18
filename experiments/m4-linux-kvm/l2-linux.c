/* Single-vCPU KVM direct64 acceptance runner. Linux boot layout is shared
 * with Syle's native loader. The guest has an in-kernel PIC/PIT and COM1. */
#include "linux_boot.h"
#include "l2-disk.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static int check(int n, const char *what) {
    if (n < 0) { perror(what); exit(1); }
    return n;
}

static void *input(const char *path, size_t *size) {
    int fd = check(open(path, O_RDONLY), path);
    struct stat st;
    check(fstat(fd, &st), "fstat");
    *size = st.st_size;
    void *p = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) { perror(path); exit(1); }
    close(fd);
    return p;
}

static bool copy_ram(void *ctx, uint64_t gpa, const void *src, size_t len) {
    if (gpa > LB_RAM_SIZE || len > LB_RAM_SIZE - gpa) return false;
    memcpy((char *)ctx + gpa, src, len);
    return true;
}

static bool zero_ram(void *ctx, uint64_t gpa, size_t len) {
    if (gpa > LB_RAM_SIZE || len > LB_RAM_SIZE - gpa) return false;
    memset((char *)ctx + gpa, 0, len);
    return true;
}

struct uart {
    unsigned char ier, lcr, mcr, scratch, dll, dlm;
    bool pending, irq;
    char line[1024];
    size_t used;
    bool earlycon, init, interactive, shutdown;
    unsigned char rx[128];
    size_t first, end;
};

static void serial_byte(struct uart *u, unsigned char v) {
    bool live = u->interactive && u->init;
    if (live) putchar(v);
    if (v == '\r') return;
    if (v != '\n' && u->used < sizeof(u->line)-1) {
        u->line[u->used++] = v;
        return;
    }
    u->line[u->used] = 0;
    if (!live) printf("L2 | %s\n", u->line);
    if (strstr(u->line,"reboot: Power off not available: System halted instead") ||
        strstr(u->line,"reboot: System halted")) u->shutdown=true;
    if (strstr(u->line,"earlycon: uart8250") || strstr(u->line,"bootconsole [uart8250] enabled"))
        u->earlycon = true;
    if (!strcmp(u->line,"SYLE_LINUX_USERSPACE_READY")) u->init = true;
    u->used = 0;
}

static void uart_irq(struct uart *u, int vm) {
    bool irq = (u->mcr & 8) && (((u->ier & 1) && u->first < u->end) ||
                                ((u->ier & 2) && u->pending));
    if (u->irq != irq) {
        struct kvm_irq_level level = {.irq=4,.level=irq};
        check(ioctl(vm,KVM_IRQ_LINE,&level),"KVM_IRQ_LINE");
        u->irq = irq;
    }
}

static struct termios saved_terminal;
/* A halted vCPU can sleep inside KVM_RUN. Interrupt that wait to service
 * the L1 terminal without a second vCPU or a busy-wait in L2. */
static volatile sig_atomic_t poll_console;
static void wake_console(int sig) { (void)sig; poll_console=1; }
static void restore_console(void) { tcsetattr(STDIN_FILENO,TCSANOW,&saved_terminal); }
static void start_console(void) {
    check(tcgetattr(STDIN_FILENO,&saved_terminal),"tcgetattr");
    struct termios raw=saved_terminal;
    cfmakeraw(&raw);
    raw.c_cc[VMIN]=0;
    raw.c_cc[VTIME]=0;
    if (atexit(restore_console)) exit(1);
    check(tcsetattr(STDIN_FILENO,TCSANOW,&raw),"tcsetattr");
    struct sigaction alarm={.sa_handler=wake_console,.sa_flags=SA_RESTART};
    sigemptyset(&alarm.sa_mask);
    check(sigaction(SIGALRM,&alarm,NULL),"sigaction");
    struct itimerval tick={.it_value={0,50000},.it_interval={0,50000}};
    check(setitimer(ITIMER_REAL,&tick,NULL),"setitimer");
}

static void uart_io(struct uart *u, int vm, unsigned reg, bool out, uint8_t *v) {
    if ((u->lcr & 128) && reg <= 1) {
        uint8_t *div = reg ? &u->dlm : &u->dll;
        if (out) *div = *v; else *v = *div;
    } else if (out) {
        switch (reg) {
        case 0: serial_byte(u,*v); u->pending = true; break;
        case 1: if ((*v & 2) && !(u->ier & 2)) u->pending = true;
                u->ier = *v & 15; break;
        case 3: u->lcr = *v; break;
        case 4: u->mcr = *v; break;
        case 7: u->scratch = *v; break;
        }
    } else {
        switch (reg) {
        case 1: *v = u->ier; break;
        case 0: *v = u->first < u->end ? u->rx[u->first++] : 0; break;
        case 2: if ((u->ier & 1) && u->first < u->end) { *v=4; break; }
                *v = (u->pending && (u->ier & 2)) ? 2 : 1; u->pending = false; break;
        case 3: *v = u->lcr; break;
        case 4: *v = u->mcr; break;
        case 5: *v = 0x60 | (u->first < u->end); break;
        case 6: *v = (u->mcr & 16) ? ((u->mcr & 12) << 4) | ((u->mcr & 2) << 3) | ((u->mcr & 1) << 5) : 0xb0; break;
        case 7: *v = u->scratch; break;
        default: *v = 0; break;
        }
    }
    uart_irq(u,vm);
}

int main(int argc,char **argv) {
    bool disk_root=argc==5 && !strcmp(argv[3],"--disk");
    bool interactive=disk_root || (argc==4 && !strcmp(argv[3],"--shell"));
    if (argc != 3 && !interactive) { fprintf(stderr,"usage: %s bzImage initramfs.cpio [--shell | --disk ROOT.img]\n",argv[0]); return 2; }
    setbuf(stdout,NULL);
    size_t ks,rs;
    void *kernel = input(argv[1],&ks), *initrd = input(argv[2],&rs);
    void *ram = mmap(NULL,LB_RAM_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (ram == MAP_FAILED) { perror("ram"); return 1; }
    struct lb_writer writer = {ram,copy_ram,zero_ram};
    struct lb_plan plan;
    struct lb_entry_state entry;
    const char *cmd = "earlycon=uart8250,io,0x3f8,115200n8 console=ttyS0,115200n8 rdinit=/init nokaslr noapic nolapic acpi=off pci=off tsc=reliable lpj=1000000";
    struct l2_disk disk={0};
    if(disk_root) {
        if(!l2_disk_open(&disk,argv[4],ram)){perror("L2 root disk");return 1;}
        cmd="earlycon=uart8250,io,0x3f8,115200n8 console=ttyS0,115200n8 root=/dev/vda rootfstype=ext2 rw rootwait init=/sbin/init virtio_mmio.device=4K@0xd0000000:5 nokaslr noapic nolapic acpi=off pci=off tsc=reliable lpj=1000000";
    }
    if (!lb_load(kernel,ks,initrd,rs,cmd,&writer,&plan) || !lb_entry_prepare(&plan,&writer,&entry)) {
        fprintf(stderr,"direct64 loader rejected image\n"); return 1;
    }
    printf("L2 direct64 rip=%llx cmdline=%s\n",(unsigned long long)entry.rip,cmd);
    int kvm=check(open("/dev/kvm",O_RDWR),"/dev/kvm");
    if (check(ioctl(kvm,KVM_GET_API_VERSION),"API") != KVM_API_VERSION) return 1;
    int vm=check(ioctl(kvm,KVM_CREATE_VM,0),"CREATE_VM");
    check(ioctl(vm,KVM_SET_TSS_ADDR,0xfffbd000UL),"SET_TSS");
    check(ioctl(vm,KVM_CREATE_IRQCHIP,0),"CREATE_IRQCHIP");
    struct kvm_pit_config pit={.flags=KVM_PIT_SPEAKER_DUMMY};
    check(ioctl(vm,KVM_CREATE_PIT2,&pit),"CREATE_PIT2");
    /* A delayed vCPU observes coalesced PIT edges, as on an 8259.
     * Avoid replaying every missed host tick before L2 can leave its ISR. */
    struct kvm_reinject_control ticks={.pit_reinject=0};
    check(ioctl(vm,KVM_REINJECT_CONTROL,&ticks),"REINJECT_CONTROL");
    struct kvm_userspace_memory_region slot={.memory_size=LB_RAM_SIZE,.userspace_addr=(uintptr_t)ram};
    check(ioctl(vm,KVM_SET_USER_MEMORY_REGION,&slot),"SET_MEMORY");
    int cpu=check(ioctl(vm,KVM_CREATE_VCPU,0),"CREATE_VCPU");
    struct kvm_cpuid2 *cpuid=calloc(1,sizeof(*cpuid)+256*sizeof(cpuid->entries[0]));
    if(!cpuid)return 1;
    cpuid->nent=256;
    check(ioctl(kvm,KVM_GET_SUPPORTED_CPUID,cpuid),"GET_SUPPORTED_CPUID");
    check(ioctl(cpu,KVM_SET_CPUID2,cpuid),"SET_CPUID2");
    free(cpuid);
    struct kvm_sregs sr;
    check(ioctl(cpu,KVM_GET_SREGS,&sr),"GET_SREGS");
    sr.cr0=entry.cr0; sr.cr3=entry.cr3; sr.cr4=entry.cr4; sr.efer=entry.efer;
    sr.gdt.base=entry.gdt_base; sr.gdt.limit=entry.gdt_limit;
    sr.cs=(struct kvm_segment){.selector=entry.cs,.limit=UINT32_MAX,.type=11,.present=1,.s=1,.l=1,.g=1};
    sr.ds=sr.es=sr.fs=sr.gs=sr.ss=(struct kvm_segment){.selector=entry.ds,.limit=UINT32_MAX,.type=3,.present=1,.s=1,.db=1,.g=1};
    sr.tr=(struct kvm_segment){.selector=32,.base=0x6000,.limit=103,.type=11,.present=1};
    check(ioctl(cpu,KVM_SET_SREGS,&sr),"SET_SREGS");
    struct kvm_regs regs={.rip=entry.rip,.rsp=entry.rsp,.rsi=entry.rsi,.rflags=2};
    check(ioctl(cpu,KVM_SET_REGS,&regs),"SET_REGS");
    int size=check(ioctl(kvm,KVM_GET_VCPU_MMAP_SIZE),"GET_VCPU_MMAP_SIZE");
    struct kvm_run *run=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,cpu,0);
    if(run==MAP_FAILED){perror("vcpu mmap");return 1;}
    struct uart u={.interactive=interactive};
    bool console_started=false;
    /* Stable CMOS boot fixture: BCD, 24-hour, January 1, 2024. */
    uint8_t cmos[128]={[6]=2,[7]=1,[8]=1,[9]=0x24,[10]=0x26,[11]=2,[13]=0x80,[0x32]=0x20};
    uint8_t cmos_index=0;
    while (!u.init || interactive) {
        if (interactive && u.init && !console_started) {
            start_console();
            console_started=true;
        }
        if (poll_console) {
            poll_console=0;
            if (u.first==u.end) {
                ssize_t n=read(STDIN_FILENO,u.rx,sizeof u.rx);
                if(n<0 && errno!=EINTR){perror("console read");return 1;}
                if(n>0){u.first=0;u.end=(size_t)n;uart_irq(&u,vm);}
            }
            if (u.shutdown) {
                struct kvm_mp_state mp;
                check(ioctl(cpu,KVM_GET_MP_STATE,&mp),"GET_MP_STATE");
                if (mp.mp_state==KVM_MP_STATE_HALTED) {
                    check(ioctl(cpu,KVM_GET_REGS,&regs),"GET_REGS");
                    if (regs.rflags & 0x200) continue;
                    puts("L2_SHUTDOWN_PASS halted=1 IF=0");
                    return 0;
                }
            }
        }
        if(ioctl(cpu,KVM_RUN,0)<0){if(errno==EINTR)continue;perror("KVM_RUN");return 1;}
        if(run->exit_reason==KVM_EXIT_IO){
            uint8_t *bytes=(uint8_t*)run+run->io.data_offset;
            bool out=run->io.direction==KVM_EXIT_IO_OUT;
            if(run->io.port>=0x3f8 && run->io.port<0x400 && run->io.size==1){
                for(unsigned i=0;i<run->io.count;i++)uart_io(&u,vm,run->io.port-0x3f8,out,bytes+i);
            } else if((run->io.port==0x70||run->io.port==0x71)&&run->io.size==1){
                for(unsigned i=0;i<run->io.count;i++){
                    if(run->io.port==0x70){
                        if(out)cmos_index=bytes[i]&127; else bytes[i]=cmos_index;
                    }else if(out){
                        if(cmos_index!=12&&cmos_index!=13)cmos[cmos_index]=bytes[i];
                    }else bytes[i]=cmos[cmos_index];
                }
            } else if(!out) memset(bytes,0xff,(size_t)run->io.size*run->io.count);
        }else if(run->exit_reason==KVM_EXIT_MMIO){
            uint64_t addr=run->mmio.phys_addr;
            if(disk_root && addr>=L2_DISK_BASE && addr-L2_DISK_BASE<4096){
                if(!l2_disk_mmio(&disk,vm,addr-L2_DISK_BASE,run->mmio.data,run->mmio.len,run->mmio.is_write)){
                    fprintf(stderr,"L2 disk MMIO failed offset=%llx write=%u len=%u\n",
                            (unsigned long long)(addr-L2_DISK_BASE),run->mmio.is_write,run->mmio.len);
                    return 1;
                }
            }else if(!run->mmio.is_write)memset(run->mmio.data,0xff,run->mmio.len);
        }else{
            check(ioctl(cpu,KVM_GET_REGS,&regs),"GET_REGS");
            fprintf(stderr,"L2 exit=%u rip=%llx\n",run->exit_reason,regs.rip);
            return 1;
        }
    }
    if(!u.earlycon)return 1;
    puts("L2_LINUX_INIT_PASS");
    return 0;
}
