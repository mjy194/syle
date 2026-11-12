#!/usr/bin/env python3
"""Host-only N47B assembly flow checks; this does not execute or prove VMX.

Assemble the original L1/L2 includes, then run the same L2 instructions with
guest addresses relocated above mmap_min_addr and VMCALL changed to a return.
The test models a barrier after each phase and independently checks every data
page and report field for all 256 within-phase owner orderings.
"""

from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
L1 = ROOT / "nested_source_replace_l1.inc"
L2 = ROOT / "nested_source_replace_l2.inc"

C_TEST = r"""
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

extern int payload_enter(void);
extern int payload_resume(void);

#define OLD UINT64_C(0x3344334433443344)
#define NEW UINT64_C(0x5566556655665566)
#define CONTROL UINT64_C(0x99aa99aa99aa99aa)
#define OLD_VALUE UINT64_C(0x47b0000000000000)
#define NEW_VALUE UINT64_C(0x47b1000000000000)
#define REPORT ((uint64_t *)UINT64_C(0x100a000))
#define PHASE (*(uint64_t *)UINT64_C(0x100c820))
#define CPU (*(uint64_t *)UINT64_C(0x100c828))
#define TARGET ((uint64_t *)UINT64_C(0x100f000))

static const unsigned keys[2][8] = {
    {0, 1, 0, 0, 1, 0, 0, 1},
    {0, 1, 2, 2, 1, 1, 0, 2}
};

static int corrupt(unsigned fault, unsigned phase, unsigned cpu,
                   uint64_t pages[3][512])
{
    switch (fault) {
    case 1: /* Old A/B sentinel. */
        if (phase == 0 && cpu == 0) { TARGET[0] ^= 1; return 1; }
        break;
    case 2: /* A stale translation still resolves the old backing. */
        if (phase == 3 && cpu == 0) {
            memcpy(TARGET, pages[0], sizeof pages[0]); return 1;
        }
        break;
    case 3: /* The replacement must not inherit the retained old slots. */
        if (phase == 3 && cpu == 0) { TARGET[0x40/8] = OLD_VALUE; return 1; }
        break;
    case 4: /* The read-only refill interval must not populate new slots. */
        if (phase == 4 && cpu == 1) { TARGET[0x180/8] = NEW_VALUE; return 1; }
        break;
    case 5: /* Unaffected C sentinel. */
        if (phase == 3 && cpu == 1) { TARGET[0] ^= 1; return 1; }
        break;
    case 6: /* Unaffected C's pre-replacement write. */
        if (phase == 7 && cpu == 1) { TARGET[0x90/8] ^= 1; return 1; }
        break;
    case 7: /* CPU1 sees CPU0's completed first write before its own. */
        if (phase == 6 && cpu == 1) { TARGET[0x140/8] ^= 1; return 1; }
        break;
    case 8: /* CPU0's final B must see CPU1's completed A write. */
        if (phase == 7 && cpu == 0) { TARGET[0x180/8] ^= 1; return 1; }
        break;
    case 9: /* Both owners' old A/B data must exist before replacement. */
        if (phase == 2 && cpu == 0) { TARGET[0x80/8] ^= 1; return 1; }
        break;
    default:
        assert(fault == 0);
    }
    return 0;
}

static void run(unsigned ordering, unsigned fault)
{
    uint64_t pages[3][512] = {{0}};
    uint64_t expected[3][512] = {{0}};
    uint64_t reports[2][8] = {{0}};
    pages[0][0] = expected[0][0] = OLD;
    pages[1][0] = expected[1][0] = NEW;
    pages[2][0] = expected[2][0] = CONTROL;
    memset(REPORT, 0, 2 * 64);
    for (unsigned phase = 0; phase < 8; ++phase) {
        for (unsigned turn = 0; turn < 2; ++turn) {
            unsigned cpu = turn ^ ((ordering >> phase) & 1);
            unsigned key = keys[cpu][phase];
            unsigned page = key == 2 ? 2 : (phase >= 3);
            PHASE = phase;
            CPU = cpu;
            memcpy(TARGET, pages[page], sizeof pages[page]);
            int fail = corrupt(fault, phase, cpu, pages);
            int result = phase ? payload_resume() : payload_enter();
            if (fail) {
                assert(result == -1);
                return;
            }
            assert(result == (int)phase);
            memcpy(pages[page], TARGET, sizeof pages[page]);

            reports[cpu][0] = 0x3200 + cpu;
            reports[cpu][1] = 0x47b00000 + cpu;
            if (key == 2)
                reports[cpu][6] = CONTROL;
            else
                reports[cpu][3] = phase >= 3 ? NEW : OLD;
            int old_write = phase < 2 || (phase == 2 && cpu == 1);
            int new_write = (phase == 5 && cpu == 0) ||
                            (phase == 6 && cpu == 1);
            if (old_write || new_write) {
                unsigned offset = (old_write ? 0x40 : 0x140) +
                                  cpu * 0x40 + key * 8;
                uint64_t value = (old_write ? OLD_VALUE : NEW_VALUE) +
                                 cpu * 0x100 + key;
                expected[page][offset / 8] = value;
                reports[cpu][4] = value;
                if (key == 2)
                    reports[cpu][7] = value;
            }
            reports[cpu][5] = phase + 1;
            if (phase == 7)
                reports[cpu][2] = 0x3200 + cpu;
            assert(memcmp(pages, expected, sizeof pages) == 0);
            assert(memcmp(REPORT, reports, sizeof reports) == 0);
        }
    }
    assert(fault == 0);
}

int main(void)
{
    void *base = (void *)UINT64_C(0x100a000);
    assert(mmap(base, 0x6000, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                -1, 0) == base);
    for (unsigned order = 0; order < 256; ++order)
        run(order, 0);
    for (unsigned fault = 1; fault <= 9; ++fault) {
        run(0, fault);
        run(255, fault);
    }
    PHASE = 8;
    CPU = 0;
    assert(payload_resume() == -1);
    PHASE = 0;
    CPU = 2;
    assert(payload_enter() == -1);
    assert(munmap(base, 0x6000) == 0);
    puts("N47B payload flow: 256 owner orderings and 20 negative cases passed");
    return 0;
}
"""


def main():
    l1 = L1.read_text()
    l2 = L2.read_text()
    with tempfile.TemporaryDirectory(prefix="n47b-payload-") as temp:
        work = Path(temp)
        original = work / "original.S"
        original.write_text(
            ".text\nl1_entry:\n" + l1 + "\nl2_entry:\n" + l2
            + "\n.Lnested_fail:\nud2\n.Lnested_done:\nud2\n"
            + '.section .note.GNU-stack,"",@progbits\n'
        )
        obj = work / "original.o"
        subprocess.run(["gcc", "-g", "-fno-pie", "-c", str(original),
                        "-o", str(obj)], check=True)
        symbols = subprocess.check_output(["nm", "-n", str(obj)], text=True)
        addresses = {
            name: int(value, 16)
            for value, kind, name in (line.split() for line in symbols.splitlines())
            if kind.upper() != "U"
        }
        calls = [addresses[f"nested_source_replace_call{i}"] for i in range(8)]
        assert calls == sorted(set(calls))
        assert addresses["nested_parallel_call"] == calls[0]
        assert addresses["nested_l2_resume"] < calls[0]
        disassembly = subprocess.check_output(["objdump", "-d", str(obj)], text=True)
        assert len(re.findall(r"\bvmcall\b", disassembly)) == 8
        assert len(re.findall(r"\bvmresume\b", disassembly)) == 1

        # Retain every instruction except guest absolute addresses and VMCALL.
        relocated = re.sub(
            r"0x(?:a000|c820|c828|f[0-9a-f]{3})\b",
            lambda match: hex(int(match[0], 16) + 0x1000000), l2)
        relocated, count = re.subn(
            r"(?m)^    vmcall$",
            "    mov %r10d,%eax\n    jmp .Ltest_return", relocated)
        assert count == 8
        table = l1[l1.index(".Lsource_replace_keys:"):]
        host = work / "host.S"
        host.write_text(
            ".text\n.global payload_enter,payload_resume\n"
            "payload_enter:\npush %r12\njmp .Ltest_entry\n"
            "payload_resume:\npush %r12\njmp nested_l2_resume\n"
            ".Ltest_entry:\n" + relocated
            + "\n.Lnested_fail:\nmov $-1,%eax\n"
            + ".Ltest_return:\npop %r12\nret\n" + table
            + '\n.section .note.GNU-stack,"",@progbits\n'
        )
        harness = work / "test.c"
        harness.write_text(C_TEST)
        executable = work / "test"
        subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-O2", "-no-pie", str(harness), str(host),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        print("Original L1/L2 assembly and all 8 exported checkpoints verified")


if __name__ == "__main__":
    main()
