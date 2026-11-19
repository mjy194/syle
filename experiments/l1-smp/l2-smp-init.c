#define _GNU_SOURCE
#include <sched.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <unistd.h>
static int pin(int cpu){
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu,&mask);
    return sched_setaffinity(0,sizeof mask,&mask)||sched_getcpu()!=cpu;
}

struct shared_work {
    atomic_int ready[2];
    uint64_t checksum[2];
};

static int work_cpu(struct shared_work *work,int cpu) {
    const int failed=pin(cpu);
    atomic_store(&work->ready[cpu],failed?-1:1);
    int sibling;
    while(!(sibling=atomic_load(&work->ready[1-cpu])))
        sched_yield();
    if(failed||sibling<0)return 1;
    uint64_t value=cpu+1;
    for(unsigned i=0;i<8000000;i++)
        value=value*UINT64_C(6364136223846793005)+1;
    work->checksum[cpu]=value;
    return 0;
}

static int work_both_cpus(void) {
    struct shared_work *work=mmap(NULL,sizeof *work,PROT_READ|PROT_WRITE,
                                  MAP_SHARED|MAP_ANONYMOUS,-1,0);
    if(work==MAP_FAILED)return 1;
    atomic_init(&work->ready[0],0);
    atomic_init(&work->ready[1],0);
    pid_t peer=fork();
    if(!peer)_exit(work_cpu(work,1));
    int status=0;
    int failed=peer<0||work_cpu(work,0);
    if(peer>0&&(waitpid(peer,&status,0)!=peer||!WIFEXITED(status)||WEXITSTATUS(status)))
        failed=1;
    if(!work->checksum[0]||!work->checksum[1]||work->checksum[0]==work->checksum[1])
        failed=1;
    munmap(work,sizeof *work);
    return failed;
}

static int run_shell(void) {
    pid_t child=fork();
    if(child==0) {
        if(setsid()<0)_exit(1);
        int tty=open("/dev/ttyS0",O_RDWR|O_NOCTTY);
        if(tty<0||ioctl(tty,TIOCSCTTY,0)<0)_exit(1);
        for(int fd=0;fd<3;fd++)
            if(dup2(tty,fd)<0)_exit(1);
        if(tty>2)close(tty);
        if(setenv("PATH","/bin",1)||setenv("PS1","L2 busybox # ",1))_exit(1);
        if(dprintf(STDOUT_FILENO,"SYLE_LINUX_USERSPACE_READY\n")<0)_exit(1);
        execl("/bin/busybox","busybox","sh","-i",(char *)NULL);
        perror("BusyBox");
        _exit(1);
    }
    int status;
    return child<0||waitpid(child,&status,0)!=child||!WIFEXITED(status)||WEXITSTATUS(status);
}

int main(void){
    int log=open("/dev/kmsg", O_WRONLY|O_CLOEXEC);
    if(log<0)return 1;
    cpu_set_t initial_mask;
    if(sched_getaffinity(0,sizeof initial_mask,&initial_mask))goto failed;
    char online[32];
    if(mount("sysfs","/sys","sysfs",0,NULL)||mount("proc","/proc","proc",0,NULL))goto failed;
    FILE *f=fopen("/sys/devices/system/cpu/online","r");
    if(!f||!fgets(online,sizeof online,f))goto failed;
    fclose(f);
    if(strcmp(online,"0-1\n"))goto failed;
    for(int cpu=0;cpu<2;cpu++){
        if(pin(cpu))goto failed;
        dprintf(log,"<5>L2_SMP_PIN actual=%d\n",cpu);
    }
    if(work_both_cpus())goto failed;
    dprintf(log,"<5>L2_SMP_WORK_PASS workers=2\n");
    dprintf(log,"<5>L2_SMP_USERSPACE_PASS\n");
    if(sched_setaffinity(0,sizeof initial_mask,&initial_mask)||run_shell())goto failed;
    reboot(RB_HALT_SYSTEM);
failed:
    perror("L2 SMP init");
    dprintf(log,"<3>L2_SMP_USERSPACE_FAIL\n");
    for(;;)pause();
}
