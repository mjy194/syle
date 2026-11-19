#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static uint64_t sample_tsc(uint64_t index)
{
    uint32_t low, high;
    register uint64_t tag __asm__("r10") = UINT64_C(0x53594c45434c4b31);
    register uint64_t sample __asm__("r11") = index;
    __asm__ volatile("lfence; rdtsc; lfence"
                     : "=a"(low), "=d"(high) : "r"(tag), "r"(sample) : "memory");
    return ((uint64_t)high << 32) | low;
}

struct clock_sample {
    uint64_t loc[2], pit[2], tsc[8];
    char source[32];
};

static int collect(struct clock_sample *state, unsigned long phase)
{
    FILE *clock = fopen("/sys/devices/system/clocksource/clocksource0/current_clocksource", "r");
    if (!clock)
        return 1;
    int fields = fscanf(clock, "%31s", state->source);
    fclose(clock);
    if (fields != 1)
        return 1;
    FILE *file = fopen("/proc/interrupts", "r");
    if (!file) {
        perror("/proc/interrupts");
        return 1;
    }
    char line[512];
    int have_loc = 0;
    while (fgets(line, sizeof line, file)) {
        if (sscanf(line, " LOC: %" SCNu64 " %" SCNu64, &state->loc[0], &state->loc[1]) == 2)
            have_loc = 1;
        sscanf(line, " 0: %" SCNu64 " %" SCNu64, &state->pit[0], &state->pit[1]);
    }
    fclose(file);
    if (!have_loc) {
        fputs("No per-CPU LAPIC timer counts in /proc/interrupts\n", stderr);
        return 1;
    }
    for (unsigned i = 0; i < 8; ++i) {
        unsigned cpu = i % 2;
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu, &mask);
        if (sched_setaffinity(0, sizeof mask, &mask) || sched_getcpu() != (int)cpu) {
            perror("pin L2 clock probe");
            return 1;
        }
        state->tsc[i] = sample_tsc(phase * 8 + i);
    }
    return 0;
}

static void report(const struct clock_sample *state, unsigned long phase)
{
    printf("L2_CLOCK_SOURCE phase=%lu source=%s\n", phase, state->source);
    printf("L2_CLOCK_IRQ phase=%lu loc0=%" PRIu64 " loc1=%" PRIu64
           " irq0cpu0=%" PRIu64 " irq0cpu1=%" PRIu64 "\n",
           phase, state->loc[0], state->loc[1], state->pit[0], state->pit[1]);
    for (unsigned i = 0; i < 8; ++i)
        printf("L2_CLOCK_TSC index=%lu cpu=%u tsc=%" PRIu64 "\n",
               phase * 8 + i, i % 2, state->tsc[i]);
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
        return 2;
    char *end;
    unsigned long phase = strtoul(argv[1], &end, 10);
    if (!phase || *end || phase > 1000)
        return 2;
    unsigned long seconds = 0;
    if (argc == 3) {
        seconds = strtoul(argv[2], &end, 10);
        if (!seconds || *end || seconds > 300)
            return 2;
    }
    struct clock_sample samples[3] = {0};
    const unsigned count = seconds ? 3 : 1;
    for (unsigned i = 0; i < count; ++i) {
        if (i) {
            struct timespec left = {.tv_sec = (time_t)seconds};
            while (nanosleep(&left, &left))
                if (errno != EINTR)
                    return 1;
        }
        if (collect(&samples[i], phase + i))
            return 1;
    }
    /* Do not put serial output, or its nested I/O exits, between samples. */
    for (unsigned i = 0; i < count; ++i)
        report(&samples[i], phase + i);
    return 0;
}
