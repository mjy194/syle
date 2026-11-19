/* Single-vCPU guest-side acceptance probe. Run on x86-64 Linux. */
#define _GNU_SOURCE
#include <cpuid.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <inttypes.h>
#include <sched.h>
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

enum { concurrent_exits = 8, scratch_offset = 0x800 };

/* The coordinator releases one KVM_RUN at a time per worker. Both workers
 * have their VM and RAM mapped before either can enter L2. */
static void exchange_byte(int fd, int sending)
{
    char token = 'x';
    ssize_t size;
    do {
        size = sending ? write(fd, &token, 1) : read(fd, &token, 1);
    } while (size < 0 && errno == EINTR);
    if (size != 1 || token != 'x') {
        fprintf(stderr, "L2 coordinator disconnected\n");
        exit(1);
    }
}

static int pin_cpu(int target)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(target, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) < 0) {
        perror("sched_setaffinity");
        return 0;
    }
    return sched_getcpu() == target;
}

int main(int argc, char **argv)
{
    int concurrent = argc == 5 && strcmp(argv[1], "--interleave") == 0;
    int migrate = argc == 2 && strcmp(argv[1], "--migrate") == 0;
    int state_probe = concurrent || migrate;
    int io_exits = migrate ? 2 : concurrent_exits;
    int worker = concurrent ? atoi(argv[2]) : 0;
    int start = concurrent ? atoi(argv[3]) : -1;
    int done = concurrent ? atoi(argv[4]) : -1;
    if (concurrent && (worker < 0 || worker > 1 || sched_getcpu() != worker))
        return 1;
    int long_mode = state_probe || (argc == 2 && strcmp(argv[1], "--long-mode") == 0);
    if (migrate && !pin_cpu(0))
        return 1;
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
    /* r9 = [0x800] + r8; ++r8; [0x800] = r9; OUT r8b; LOOP; HLT. */
    const uint8_t interleaved[] = {
        0x4c,0x8b,0x0c,0x25,0x00,0x08,0x00,0x00, 0x4d,0x01,0xc1,
        0x49,0xff,0xc0, 0x4c,0x89,0x0c,0x25,0x00,0x08,0x00,0x00,
        0x44,0x89,0xc0, 0xe6,0xe9, 0xe2,0xe3, 0xf4
    };
    uint64_t *scratch = (uint64_t *)(ram + scratch_offset);
    uint64_t expected_r8 = worker ? UINT64_C(0x9876543200000060) : UINT64_C(0x1234567800000040);
    uint64_t cookie = expected_r8 ^ UINT64_C(0x2a98dde414f031da);
    uint64_t expected_memory = ~cookie;
    if (long_mode) {
        if (state_probe) {
            memcpy(ram, interleaved, sizeof(interleaved));
            scratch[0] = expected_memory;
            scratch[1] = cookie;
        } else {
            memcpy(ram, code64, sizeof(code64));
        }
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
    if (state_probe) {
        regs.r8 = expected_r8;
        regs.r10 = cookie;
        regs.rcx = io_exits;
    }
    checked(ioctl(cpu, KVM_SET_REGS, &regs), "KVM_SET_REGS");
    if (state_probe) {
        if (concurrent) {
            printf("SMP_L2_READY cpu=%d api=%d vmx=%u\n", worker, version, (c >> 5) & 1);
            exchange_byte(done, 1);
        }
        for (int step = 0; step <= io_exits; ++step) {
            int target = migrate ? step % 2 : worker;
            if (concurrent)
                exchange_byte(start, 0);
            if (migrate && !pin_cpu(target))
                return 1;
            if (sched_getcpu() != target)
                return 1;
            if (migrate)
                printf("SMP_L2_MIGRATE_ENTER step=%d cpu=%d fd=%d\n", step, target, cpu);
            int result;
            do { result = ioctl(cpu, KVM_RUN, 0); } while (result < 0 && errno == EINTR);
            checked(result, "KVM_RUN");
            checked(ioctl(cpu, KVM_GET_REGS, &regs), "KVM_GET_REGS");
            int hlt = step == io_exits;
            if (!hlt) {
                expected_memory += expected_r8++;
                if (run->exit_reason != KVM_EXIT_IO || run->io.direction != KVM_EXIT_IO_OUT ||
                    run->io.port != 0xe9 || run->io.size != 1 || run->io.count != 1 ||
                    run->io.data_offset >= (unsigned)size ||
                    ((uint8_t *)run)[run->io.data_offset] != (uint8_t)expected_r8)
                    return 1;
            } else if (run->exit_reason != KVM_EXIT_HLT) {
                return 1;
            }
            /* KVM completes OUT and advances RIP on the next KVM_RUN. */
            if (regs.r8 != expected_r8 || regs.r9 != expected_memory || regs.r10 != cookie ||
                regs.rip != sizeof(interleaved) - (hlt ? 0 : 5) ||
                regs.rcx != (unsigned)(io_exits - step) ||
                scratch[0] != expected_memory || scratch[1] != cookie ||
                sched_getcpu() != target) {
                fprintf(stderr, "L2 state mismatch cpu=%d round=%d rip=%llx r8=%llx r9=%llx r10=%llx rcx=%llx mem=%" PRIx64 "\n",
                        target, step, regs.rip, regs.r8, regs.r9, regs.r10, regs.rcx, scratch[0]);
                return 1;
            }
            if (concurrent) {
                printf("SMP_L2_EXIT cpu=%d round=%d exit=%s r8=%016" PRIx64 " mem=%016" PRIx64 "\n",
                       target, step, hlt ? "HLT" : "IO", expected_r8, scratch[0]);
                exchange_byte(done, 1);
            } else {
                printf("SMP_L2_MIGRATE_EXIT step=%d cpu=%d fd=%d exit=%s rip=%llx rcx=%llx "
                       "r8=%016llx r9=%016llx r10=%016llx mem=%016" PRIx64 " sentinel=%016" PRIx64 "\n",
                       step, target, cpu, hlt ? "HLT" : "IO", regs.rip, regs.rcx, regs.r8, regs.r9,
                       regs.r10, scratch[0], scratch[1]);
            }
        }
        if (migrate)
            puts("SMP_L2_MIGRATE_PASS");
        return 0;
    }
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
