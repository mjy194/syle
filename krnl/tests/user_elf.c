#include "../user_elf.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char image[16384], code[4096], data[4096];
static void put16(size_t off, uint16_t v) { memcpy(image + off, &v, 2); }
static void put32(size_t off, uint32_t v) { memcpy(image + off, &v, 4); }
static void put64(size_t off, uint64_t v) { memcpy(image + off, &v, 8); }
static void reset(void) {
    memset(image, 0, sizeof image);
    memcpy(image, "\177ELF\2\1\1", 7);
    put16(16, 2); put16(18, 62); put32(20, 1);
    put64(24, SYLE_USER_BASE); put64(32, 64);
    put16(52, 64); put16(54, 56); put16(56, 1);
    put32(64, 1); put32(68, 5); put64(72, 4096);
    put64(80, SYLE_USER_BASE); put64(96, 1); put64(104, 64); put64(112, 4096);
    image[4096] = 0xcc;
}
static int load(size_t size) {
    uint64_t entry = 0;
    return user_elf_load(image, size, code, data, &entry);
}
#define REJECT(change) do { reset(); change; assert(!load(sizeof image)); } while (0)
int main(int argc, char **argv) {
    reset(); assert(load(sizeof image));
    assert(code[0] == 0xcc && code[1] == 0 && data[0] == 0);
    for (size_t n = 0; n < 120; ++n) assert(!load(n));
    REJECT(image[4] = 1); REJECT(image[5] = 2); REJECT(put16(16, 3));
    REJECT(put16(18, 3)); REJECT(put16(54, 55)); REJECT(put16(56, 17));
    REJECT(put64(32, UINT64_MAX)); REJECT(put64(72, UINT64_MAX));
    REJECT(put64(96, UINT64_MAX)); REJECT(put64(104, 4097));
    REJECT(put64(104, 0)); REJECT(put32(68, 7)); REJECT(put32(68, 4));
    REJECT(put64(80, SYLE_USER_BASE + 1)); REJECT(put64(24, SYLE_USER_BASE + 1));
    REJECT(put64(112, 3)); REJECT(put64(72, 4097)); REJECT(put32(64, 3));
    REJECT(put16(56, 2); memcpy(image + 120, image + 64, 56));
    reset(); put16(56, 2); memcpy(image + 120, image + 64, 56);
    put32(124, 6); put64(128, 8192); put64(136, SYLE_USER_BASE + 4096);
    image[8192] = 0x42;
    assert(load(sizeof image)); assert(data[0] == 0x42 && data[1] == 0);
    /* Invalid trailing headers must not partially overwrite pages. */
    memset(code, 0xaa, sizeof code); put32(120, 2);
    assert(!load(sizeof image)); assert(code[0] == 0xaa);
    if (argc == 2) {
        FILE *f = fopen(argv[1], "rb"); assert(f);
        size_t n = fread(image, 1, sizeof image, f); assert(feof(f)); fclose(f);
        assert(load(n));
    }
    puts("PASS bounded ELF64 loader: valid code/data, truncation, bounds, entry, flags, segments");
}
