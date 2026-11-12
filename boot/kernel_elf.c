#include "kernel_elf.h"
#define PAGE UINT64_C(4096)
#define PHDR_LIMIT 16u
struct elf_header {
    unsigned char ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct program_header {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
};
struct checked_image {
    struct syle_kernel_plan plan;
    struct program_header loads[PHDR_LIMIT];
    unsigned count;
};
_Static_assert(sizeof(struct elf_header) == 64, "ELF64 header size");
_Static_assert(sizeof(struct program_header) == 56, "ELF64 phdr size");
static void copy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
}
static uint64_t floor_page(uint64_t v) { return v & ~(PAGE - 1); }
static uint64_t ceil_page(uint64_t v) { return (v + PAGE - 1) & ~(PAGE - 1); }
static int inspect(const void *image, size_t bytes, struct checked_image *checked) {
    if (!image || bytes < sizeof(struct elf_header) || bytes > SYLE_KERNEL_MAX_FILE ||
        (uintptr_t)image > UINTPTR_MAX - bytes) return -1;
    struct elf_header h;
    copy(&h, image, sizeof h);
    if (h.ident[0] != 0x7f || h.ident[1] != 'E' || h.ident[2] != 'L' ||
        h.ident[3] != 'F' || h.ident[4] != 2 || h.ident[5] != 1 ||
        h.ident[6] != 1 || h.type != 2 || h.machine != 62 || h.version != 1 ||
        h.flags || h.ehsize != sizeof h || h.phentsize != sizeof(struct program_header) ||
        !h.phnum || h.phnum > PHDR_LIMIT || h.phoff < sizeof h || h.phoff > bytes ||
        h.phnum > (bytes - h.phoff) / sizeof(struct program_header)) return -1;
    uint64_t lowest = UINT64_MAX, highest = 0;
    int executable_entry = 0, stack_seen = 0;
    checked->count = 0;
    for (unsigned i = 0; i < h.phnum; ++i) {
        struct program_header p;
        copy(&p, (const unsigned char *)image + h.phoff + i * sizeof p, sizeof p);
        if (p.type == 0) continue;
        if (p.type == 0x6474e551) {
            if (p.flags != 6 || stack_seen++) return -1;
            continue;
        }
        if (p.type != 1 || !p.memsz || p.filesz > p.memsz ||
            p.offset > bytes || p.filesz > bytes - p.offset ||
            p.vaddr != p.paddr || p.vaddr < SYLE_KERNEL_BASE ||
            p.vaddr >= SYLE_KERNEL_BASE + SYLE_KERNEL_MAX_SPAN ||
            p.memsz > SYLE_KERNEL_BASE + SYLE_KERNEL_MAX_SPAN - p.vaddr ||
            (p.flags != 4 && p.flags != 5 && p.flags != 6) ||
            (p.align > 1 && ((p.align & (p.align - 1)) ||
             (p.offset & (p.align - 1)) != (p.vaddr & (p.align - 1))))) return -1;
        uint64_t start = floor_page(p.vaddr), end = ceil_page(p.vaddr + p.memsz);
        for (unsigned j = 0; j < checked->count; ++j) {
            const struct program_header *q = &checked->loads[j];
            if (start < ceil_page(q->vaddr + q->memsz) && end > floor_page(q->vaddr))
                return -1; /* No shared pages, even if permission bits agree. */
        }
        checked->loads[checked->count++] = p;
        if (start < lowest) lowest = start;
        if (end > highest) highest = end;
        if ((p.flags & 1) && h.entry >= p.vaddr && h.entry - p.vaddr < p.filesz)
            executable_entry = 1;
    }
    if (!checked->count || lowest != SYLE_KERNEL_BASE || !executable_entry ||
        highest <= lowest || highest - lowest > SYLE_KERNEL_MAX_SPAN) return -1;
    checked->plan.base = lowest;
    checked->plan.size = highest - lowest;
    checked->plan.entry = h.entry;
    return 0;
}
int syle_kernel_elf_inspect(const void *image, size_t bytes, struct syle_kernel_plan *out) {
    struct checked_image checked;
    if (!out || inspect(image, bytes, &checked)) return -1;
    *out = checked.plan;
    return 0;
}
int syle_kernel_elf_load(const void *image, size_t bytes, void *destination,
                       size_t capacity, struct syle_kernel_plan *out) {
    struct checked_image checked;
    if (!destination || !out || inspect(image, bytes, &checked) ||
        checked.plan.size > capacity ||
        (uintptr_t)destination > UINTPTR_MAX - checked.plan.size) return -1;
    uintptr_t src = (uintptr_t)image, dst = (uintptr_t)destination;
    if (src < dst + checked.plan.size && dst < src + bytes) return -1;
    unsigned char *d = destination;
    for (size_t i = 0; i < checked.plan.size; ++i) d[i] = 0;
    for (unsigned i = 0; i < checked.count; ++i) {
        const struct program_header *p = &checked.loads[i];
        copy(d + (p->vaddr - checked.plan.base),
             (const unsigned char *)image + p->offset, p->filesz);
    }
    *out = checked.plan;
    return 0;
}
