/* Single-vCPU guest-side acceptance probe. Run on x86-64 Linux. */
#include <cpuid.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int checked(int value, const char *operation)
{
    if (value < 0) {
        perror(operation);
        exit(1);
    }
    return value;
}

int main(int argc, char **argv)
{
    int long_mode = argc == 2 && strcmp(argv[1], "--long-mode") == 0;
    unsigned int a, b, c, d;
    setbuf(stdout, NULL);
    __cpuid(1, a, b, c, d);
    printf("KVM_PROBE cpuid.1.ecx=%08x vmx=%u\n", c, (c >> 5) & 1);

    int kvm = checked(open("/dev/kvm", O_RDWR | O_CLOEXEC), "open /dev/kvm");
    int version = checked(ioctl(kvm, KVM_GET_API_VERSION, 0), "KVM_GET_API_VERSION");
    if (version != KVM_API_VERSION) {
        fprintf(stderr, "KVM API %d, expected %d\n", version, KVM_API_VERSION);
        return 1;
    }
    printf("KVM_GET_API_VERSION=%d\n", version);
    if (argc == 2 && strcmp(argv[1], "--check-device") == 0) {
        checked(close(kvm), "close /dev/kvm");
        return 0;
    }
    int vm = checked(ioctl(kvm, KVM_CREATE_VM, 0), "KVM_CREATE_VM");
    checked(ioctl(vm, KVM_SET_TSS_ADDR, 0xfffbd000UL), "KVM_SET_TSS_ADDR");
    size_t page_size = long_mode ? 5 * 4096 : 4096;
    uint8_t *ram = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ram == MAP_FAILED) { perror("mmap guest RAM"); return 1; }
    /* MOV AL,'A'; OUT 0xe9,AL; INC AX; OUT 0xe9,AL; HLT */
    const uint8_t code[] = {0xb0, 0x41, 0xe6, 0xe9, 0x40, 0xe6, 0xe9, 0xf4};
    /* MOVABS R8,imm64; MOV RAX,R8; OUT; INC R8; MOV RAX,R8; OUT; HLT. */
    const uint8_t code64[] = {0x49,0xb8,0x41,0,0,0,0x78,0x56,0x34,0x12,
                              0x4c,0x89,0xc0,0xe6,0xe9,0x49,0xff,0xc0,
                              0x4c,0x89,0xc0,0xe6,0xe9,0xf4};
    if (long_mode) {
        memcpy(ram, code64, sizeof(code64));
        /* Four levels identity-map the code page at linear address zero. */
        for (unsigned level = 1; level <= 4; level++)
            *(uint64_t *)(ram + 4096 * level) = (level == 4 ? 0 : 4096 * (level + 1)) | 3;
    } else {
        memcpy(ram, code, sizeof(code));
    }
    struct kvm_userspace_memory_region region = {
        .memory_size = page_size, .userspace_addr = (uintptr_t)ram,
    };
    checked(ioctl(vm, KVM_SET_USER_MEMORY_REGION, &region), "KVM_SET_USER_MEMORY_REGION");
    int cpu = checked(ioctl(vm, KVM_CREATE_VCPU, 0), "KVM_CREATE_VCPU");
    if (long_mode) {
        enum { max_entries = 256 };
        struct kvm_cpuid2 *cpuid = calloc(1, sizeof(*cpuid) +
                                             max_entries * sizeof(cpuid->entries[0]));
        if (!cpuid) { perror("calloc CPUID"); return 1; }
        cpuid->nent = max_entries;
        checked(ioctl(kvm, KVM_GET_SUPPORTED_CPUID, cpuid), "KVM_GET_SUPPORTED_CPUID");
        checked(ioctl(cpu, KVM_SET_CPUID2, cpuid), "KVM_SET_CPUID2");
        free(cpuid);
    }
    int size = checked(ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, 0), "KVM_GET_VCPU_MMAP_SIZE");
    struct kvm_run *run = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, cpu, 0);
    if (run == MAP_FAILED) { perror("mmap kvm_run"); return 1; }
    struct kvm_sregs sregs;
    checked(ioctl(cpu, KVM_GET_SREGS, &sregs), "KVM_GET_SREGS");
    sregs.cs.selector = 0;
    sregs.cs.base = 0;
    if (long_mode) {
        sregs.cr3 = 4096;
        sregs.cr4 = 1u << 5; /* PAE */
        sregs.cr0 |= (1u << 31) | 1; /* PG, PE */
        sregs.efer = (1u << 8) | (1u << 10); /* LME, LMA */
        sregs.cs = (struct kvm_segment){.selector=8, .limit=UINT32_MAX, .type=11,
                                       .present=1, .s=1, .l=1, .g=1};
        sregs.ss = (struct kvm_segment){.selector=16, .limit=UINT32_MAX, .type=3,
                                       .present=1, .s=1, .db=1, .g=1};
        sregs.ds = sregs.es = sregs.ss;
    }
    checked(ioctl(cpu, KVM_SET_SREGS, &sregs), "KVM_SET_SREGS");
    struct kvm_regs regs = {.rip = 0, .rflags = 2};
    checked(ioctl(cpu, KVM_SET_REGS, &regs), "KVM_SET_REGS");
    for (int step = 0; step < 3; ++step) {
        int result;
        do { result = ioctl(cpu, KVM_RUN, 0); } while (result < 0 && errno == EINTR);
        checked(result, "KVM_RUN");
        printf("KVM_RUN %d exit=%u\n", step + 1, run->exit_reason);
        if (step == 2) {
            if (run->exit_reason != KVM_EXIT_HLT) return 1;
        } else {
            if (run->exit_reason != KVM_EXIT_IO || run->io.direction != KVM_EXIT_IO_OUT ||
                run->io.port != 0xe9 || run->io.size != 1 || run->io.count != 1 ||
                run->io.data_offset >= (unsigned)size) return 1;
            uint8_t byte = ((uint8_t *)run)[run->io.data_offset];
            printf("L2 OUT %c\n", byte);
            if (byte != 'A' + step) return 1;
        }
    }
    if (long_mode) {
        checked(ioctl(cpu, KVM_GET_REGS, &regs), "KVM_GET_REGS");
        printf("L2_64 RIP=%llx R8=%llx RAX=%llx\n", regs.rip, regs.r8, regs.rax);
        if (regs.rip != sizeof(code64) || regs.r8 != UINT64_C(0x1234567800000042) ||
            regs.rax != regs.r8) return 1;
        puts("KVM_L2_64_RESUME_PASS");
    } else {
        puts("KVM_L2_RESUME_PASS");
    }
    return 0;
}
