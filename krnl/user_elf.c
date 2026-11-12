#include "user_elf.h"
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
_Static_assert(sizeof(struct elf_header) == 64, "ELF64 header");
_Static_assert(sizeof(struct program_header) == 56, "ELF64 phdr");
static void copy(void *to, const void *from, size_t n) {
    unsigned char *d = to;
    const unsigned char *s = from;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
}
int user_elf_load(const void *image, size_t size, unsigned char *code,
                  unsigned char *data, uint64_t *entry) {
    if (!image || !code || !data || !entry || size < sizeof(struct elf_header)) return 0;
    struct elf_header h;
    copy(&h, image, sizeof h); /* unaligned file input is allowed */
    if (h.ident[0] != 0x7f || h.ident[1] != 'E' || h.ident[2] != 'L' ||
        h.ident[3] != 'F' || h.ident[4] != 2 || h.ident[5] != 1 ||
        h.ident[6] != 1 || h.type != 2 || h.machine != 62 || h.version != 1 ||
        h.ehsize != sizeof h || h.phentsize != sizeof(struct program_header) ||
        !h.phnum || h.phnum > 16 || h.phoff < sizeof h || h.phoff > size ||
        h.phnum > (size - h.phoff) / sizeof(struct program_header)) return 0;
    unsigned present = 0;
    int executable_entry = 0;
    /* Validate the entire file before writing output pages. */
    for (unsigned i = 0; i < h.phnum; ++i) {
        struct program_header p;
        copy(&p, (const unsigned char *)image + h.phoff + i * sizeof p, sizeof p);
        if (p.type == 0) continue; /* PT_NULL */
        if (p.type == 0x6474e551) { /* PT_GNU_STACK: only RW/NX */
            if (p.flags != 6) return 0;
            continue;
        }
        if (p.type != 1 || !p.memsz || p.filesz > p.memsz || p.offset > size ||
            p.filesz > size - p.offset || p.memsz > SYLE_USER_PAGE ||
            (p.align > 1 && ((p.align & (p.align - 1)) ||
             (p.offset & (p.align - 1)) != (p.vaddr & (p.align - 1))))) return 0;
        unsigned slot;
        if (p.vaddr == SYLE_USER_BASE && p.flags == 5) slot = 0;
        else if (p.vaddr == SYLE_USER_BASE + SYLE_USER_PAGE && p.flags == 6) slot = 1;
        else return 0;
        if (present & (1u << slot)) return 0;
        present |= 1u << slot;
        if (!slot && h.entry >= p.vaddr && h.entry - p.vaddr < p.filesz)
            executable_entry = 1;
    }
    if (!(present & 1) || !executable_entry) return 0;
    for (size_t i = 0; i < SYLE_USER_PAGE; ++i) code[i] = data[i] = 0;
    for (unsigned i = 0; i < h.phnum; ++i) {
        struct program_header p;
        copy(&p, (const unsigned char *)image + h.phoff + i * sizeof p, sizeof p);
        if (p.type == 1)
            copy(p.vaddr == SYLE_USER_BASE ? code : data,
                 (const unsigned char *)image + p.offset, p.filesz);
    }
    *entry = h.entry;
    return 1;
}
