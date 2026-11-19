#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>
#ifdef SMP_NESTED_PROBE
#include <sys/wait.h>
#ifdef SMP_CONCURRENT_PROBE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#endif
#endif

#ifdef SMP_CONCURRENT_PROBE
static int exchange_byte(int fd, int sending)
{
    char token = 'x';
    ssize_t n;
    do {
        n = sending ? write(fd, &token, 1) : read(fd, &token, 1);
    } while (n < 0 && errno == EINTR);
    return n == 1 && token == 'x';
}

static int run_concurrent(void)
{
    int start[2] = {-1, -1}, done[2] = {-1, -1};
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        return 0;
    pid_t children[2] = {-1, -1};
    int success = 0;
    for (int cpu = 0; cpu < 2; ++cpu) {
        int request[2], reply[2];
        if (pipe2(request, O_CLOEXEC) < 0) {
            perror("pipe2");
            goto cleanup;
        }
        if (pipe2(reply, O_CLOEXEC) < 0) {
            perror("pipe2");
            close(request[0]);
            close(request[1]);
            goto cleanup;
        }
        start[cpu] = request[1];
        done[cpu] = reply[0];
        children[cpu] = fork();
        if (children[cpu] == 0) {
            cpu_set_t mask;
            CPU_ZERO(&mask);
            CPU_SET(cpu, &mask);
            if (sched_setaffinity(0, sizeof(mask), &mask) < 0 ||
                fcntl(request[0], F_SETFD, 0) < 0 || fcntl(reply[1], F_SETFD, 0) < 0) {
                perror("prepare pinned worker");
                _exit(1);
            }
            char worker[8], request_fd[24], reply_fd[24];
            snprintf(worker, sizeof(worker), "%d", cpu);
            snprintf(request_fd, sizeof(request_fd), "%d", request[0]);
            snprintf(reply_fd, sizeof(reply_fd), "%d", reply[1]);
            execl("/sbin/smp-l2-probe", "smp-l2-probe", "--interleave",
                  worker, request_fd, reply_fd, (char *)NULL);
            perror("exec L2 probe");
            _exit(1);
        }
        close(request[0]);
        close(reply[1]);
        if (children[cpu] < 0) {
            perror("fork");
            goto cleanup;
        }
    }
    for (int cpu = 0; cpu < 2; ++cpu)
        if (!exchange_byte(done[cpu], 0))
            goto cleanup;
    /* Both KVM_RUN calls are released before waiting for their first exits. */
    if (!exchange_byte(start[0], 1) || !exchange_byte(start[1], 1) ||
        !exchange_byte(done[0], 0) || !exchange_byte(done[1], 0))
        goto cleanup;
    /* Alternate the remaining resumes while both L2s are still live. */
    for (int round = 1; round <= 8; ++round)
        for (int cpu = 0; cpu < 2; ++cpu)
            if (!exchange_byte(start[cpu], 1) || !exchange_byte(done[cpu], 0))
                goto cleanup;
    success = 1;
cleanup:
    for (int cpu = 0; cpu < 2; ++cpu) {
        if (start[cpu] >= 0)
            close(start[cpu]);
        if (done[cpu] >= 0)
            close(done[cpu]);
    }
    for (int cpu = 0; cpu < 2; ++cpu) {
        if (children[cpu] > 0) {
            int status;
            pid_t pid;
            do {
                pid = waitpid(children[cpu], &status, 0);
            } while (pid < 0 && errno == EINTR);
            if (pid != children[cpu] || !WIFEXITED(status) || WEXITSTATUS(status))
                success = 0;
        }
    }
    return success;
}
#endif

int main(void)
{
    char online[64];
    setbuf(stdout, NULL);
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) < 0) {
        perror("mount sysfs");
        goto done;
    }
    FILE *file = fopen("/sys/devices/system/cpu/online", "r");
    if (!file || !fgets(online, sizeof(online), file)) {
        perror("read online CPUs");
        goto done;
    }
    fclose(file);
    online[strcspn(online, "\n")] = 0;
    printf("SMP_ONLINE=%s\n", online);
    for (int cpu = 0; cpu < 2; ++cpu) {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu, &mask);
        if (sched_setaffinity(0, sizeof(mask), &mask) < 0) {
            perror("sched_setaffinity");
            goto done;
        }
        int current = sched_getcpu();
        printf("SMP_PIN requested=%d actual=%d\n", cpu, current);
        if (current != cpu)
            goto done;
#if defined(SMP_NESTED_PROBE) && !defined(SMP_CONCURRENT_PROBE) && !defined(SMP_MIGRATION_PROBE)
        pid_t child = fork();
        if (child == 0) {
            execl("/sbin/smp-l2-probe", "smp-l2-probe", "--long-mode", (char *)NULL);
            perror("exec L2 probe");
            _exit(1);
        }
        int status;
        if (child < 0 || waitpid(child, &status, 0) != child ||
            !WIFEXITED(status) || WEXITSTATUS(status) || sched_getcpu() != cpu)
            goto done;
        printf("SMP_KVM_L2_RESUME_PASS cpu=%d\n", cpu);
#endif
    }
    if (strcmp(online, "0-1") == 0) {
#ifdef SMP_CONCURRENT_PROBE
        if (!run_concurrent())
            goto done;
        puts("SMP_L2_INTERLEAVE_PASS");
#endif
#ifdef SMP_MIGRATION_PROBE
        pid_t child = fork();
        if (child == 0) {
            execl("/sbin/smp-l2-probe", "smp-l2-probe", "--migrate", (char *)NULL);
            perror("exec L2 migration probe");
            _exit(1);
        }
        int status;
        if (child < 0 || waitpid(child, &status, 0) != child ||
            !WIFEXITED(status) || WEXITSTATUS(status))
            goto done;
#endif
        puts("SMP_DUAL_PASS");
        goto idle;
    }
done:
    puts("SMP_DUAL_FAIL");
idle:
    for (;;)
        pause();
}
